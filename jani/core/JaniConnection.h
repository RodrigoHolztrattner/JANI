////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////

#include <cstdint>
#include <optional>
#include <functional>
#include <iostream>
#include <fstream>
#include <limits>
#include <string>
#include <entityx/entityx.h>
#include <boost/pfr.hpp>
#include <magic_enum.hpp>
#include <ctti/type_id.hpp>
#include <ctti/static_value.hpp>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include "span.hpp"

#include <ikcp.h>

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/optional.hpp>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#define TRUE 1
#define FALSE 0
#endif

#undef max
#undef min

namespace Jani
{

    class RequestMaker;
    class RequestManager;

    template <typename ClientHashType = uint64_t, uint32_t IntervalUpdateTime = 20, uint32_t MaximumDatagramSize = 2048>
    class Connection
    {
    public:

        using ClientHash = ClientHashType;
        using ReceiveCallback = std::function<void(std::optional<ClientHash>, nonstd::span<char>)>;
        using TimeoutCallback = std::function<void(std::optional<ClientHash>)>;

#ifdef _WIN32
        using socklen_t = int;
#endif

#ifdef _WIN32
        using SocketType = SOCKET;
#else
        using SocketType = int;
#endif

    private:

        static const uint32_t MaximumDatagramSize = 2048;

        struct ClientInfo
        {
            ClientHash                                         hash = std::numeric_limits<ClientHash>::max();
            ikcpcb*                                            kcp_instance = nullptr;
            std::chrono::time_point<std::chrono::steady_clock> last_receive_timestamp = std::chrono::steady_clock::now();
            std::string                                        address;
            uint16_t                                           port = std::numeric_limits<uint16_t>::max();
            mutable bool                                       timed_out = false;
            SocketType*                                        socket = nullptr;
            struct sockaddr_in                                 client_addr;
        };

        struct ServerInfo
        {
            struct sockaddr_in server_addr;
            SocketType* socket = nullptr;
        };

    public:

        /*
        * Setup a client connection type
        * This connection will only be allowed to send data to the dst address/port
        * This connection will only be allowed to receive data from the dst address/port
        */
        Connection(int _local_port, int _dst_port, const char* _dst_address)
        {
            m_is_server = false;
            m_local_port = _local_port;
            m_dst_port = _dst_port;
            m_dst_address = _dst_address;

            if (m_local_port == 0
                || m_dst_port == 0
                || m_dst_address.length() == 0)
            {
                return;
            }


#ifdef _WIN32
            if (WSAStartup(MAKEWORD(2, 2), &m_wsa_data) != 0)
            {
                return;
            }
#endif

            m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

            //set up address to use for sending
            memset(&m_server_addr, 0, sizeof(m_server_addr));
            m_server_addr.sin_family = AF_INET;
            m_server_addr.sin_addr.s_addr = inet_addr(_dst_address);
            m_server_addr.sin_port = htons(_dst_port);

            if (!SetupListenSocket())
            {
                return;
            }

            m_single_kcp_instance = ikcp_create(0, &m_server_info);
            if (!m_single_kcp_instance)
            {
                return;
            }

            ikcp_nodelay(m_single_kcp_instance, 1, IntervalUpdateTime, 2, 1);

            {
                struct sockaddr_in outaddr;
                memset(&outaddr, 0, sizeof(outaddr));
                outaddr.sin_family = AF_INET;
                outaddr.sin_addr.s_addr = inet_addr(m_dst_address.c_str());
                outaddr.sin_port = htons(m_dst_port);

                m_server_info = { std::move(outaddr), &m_socket };
            }

            ikcp_setoutput(
                m_single_kcp_instance,
                [](const char* buf, int len, ikcpcb* kcp, void* user) -> int
                {
                    ServerInfo& server_info = *(ServerInfo*)user;

                    return sendto(*server_info.socket, buf, len, 0, (struct sockaddr*) & server_info.server_addr, sizeof(server_info.server_addr));
                });

            m_is_valid = true;
        }

        /*
        * Setup a server connection type
        * This connection will be allowed to send data to any registered client
        * This connection will be able to receive data from any client that knows its
        * address/port
        */
        Connection(int _local_port)
        {
            m_is_server = true;
            m_local_port = _local_port;

            if (m_local_port == 0)
            {
                return;
            }

#ifdef _WIN32
            if (WSAStartup(MAKEWORD(2, 2), &m_wsa_data) != 0)
            {
                return;
            }
#endif

            m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

            if (!SetupListenSocket())
            {
                return;
            }

            m_is_valid = true;
        }

        ~Connection()
        {
#if _WIN32
            closesocket(m_socket);
            WSACleanup();
#else
            close(m_socket);
#endif

            if (m_single_kcp_instance)
            {
                ikcp_release(m_single_kcp_instance);
            }

            for (auto& [client_hash, client_info] : m_server_clients)
            {
                ikcp_release(client_info.kcp_instance);
            }
        }

        /*
        * Set the minimum time (in ms) required to detect a timed out connection
        */
        void SetTimeoutTime(uint32_t _timeout_time)
        {
            m_timeout_ms = _timeout_time;
        }

        /*
        * Set the minimum window time (in ms) that is required on a connection 
        * without recent datagrams to generate a heartbeat/ping/keep-alive packet
        */
        void SetRequiredTimeForHeartbeat(uint32_t _heartbeat_time)
        {
            m_ping_window_ms = _heartbeat_time;
        }

        /*
        * This function will attempt to grab any received datagram from the UDP layer and
        * pass it to the corresponding kcp instance
        * If this is a client, it will also check if a ping is required to keep the
        * connection alive
        * This function returns the minimum time (in ms) that can be waited until a 
        * connected client or the server requires an update
        */
        uint32_t Update()
        {
            auto time_now              = std::chrono::steady_clock::now();
            auto total_time_elapsed    = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
            uint32_t minimum_wait_time = IntervalUpdateTime;

            // It's client job to ping the server and not the opposite
            if (!m_is_server && !m_is_waiting_for_ping)
            {
                auto time_now = std::chrono::steady_clock::now();
                auto time_from_last_receive_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_server_receive_timestamp).count();
                auto time_from_last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
                auto time_elapsed_for_ping_ms = time_from_last_receive_ms - time_from_last_update_ms;

                if (time_elapsed_for_ping_ms > m_ping_window_ms)
                {
                    uint32_t ping_datagram_size = 0;
                    auto* ping_datagram = GetPingDatagram(ping_datagram_size);
                    m_is_waiting_for_ping = Send(ping_datagram, ping_datagram_size);
                }
            }

            TryReceiveDatagrams();

            // Update the kcp instance(s)
            if (m_is_server)
            {
                // For each registered client
                for (auto& [client_hash, client_info] : m_server_clients)
                {
                    auto target_update_time = ikcp_check(client_info.kcp_instance, total_time_elapsed);
                    auto time_remaining_for_update = target_update_time - total_time_elapsed;

                    minimum_wait_time = std::min(minimum_wait_time, static_cast<uint32_t>(time_remaining_for_update));

                    if (time_remaining_for_update <= 0)
                    {
                        ikcp_update(client_info.kcp_instance, total_time_elapsed);
                    }
                }
            }
            else
            {
                auto target_update_time = ikcp_check(m_single_kcp_instance, total_time_elapsed);
                auto time_remaining_for_update = target_update_time - total_time_elapsed;

                minimum_wait_time = std::min(minimum_wait_time, static_cast<uint32_t>(time_remaining_for_update));

                if (time_remaining_for_update <= 0)
                {
                    ikcp_update(m_single_kcp_instance, total_time_elapsed);
                }
            }

            m_last_update_timestamp = std::chrono::steady_clock::now();

            return minimum_wait_time;
        }

        /*
        * Check if the server or any client timed-out
        * If this is a server the callback function will have the timed-out client hash, else
        * if this is a client it will not have any value (but just by calling the callback it
        * means that the server timed-out)
        */
        void DidTimeout(const TimeoutCallback& _timeout_callback) const
        {
            auto time_now = std::chrono::steady_clock::now();

            if (m_is_server)
            {
                uint64_t total_wait_time = 0;

                for (auto& [client_hash, client_info] : m_server_clients)
                {
                    auto time_from_last_receive_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - client_info.last_receive_timestamp).count();
                    auto time_from_last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
                    auto time_elapsed_for_timeout_ms = time_from_last_receive_ms - time_from_last_update_ms;

                    total_wait_time += time_elapsed_for_timeout_ms;

                    if (time_elapsed_for_timeout_ms > m_timeout_ms)
                    {
                        client_info.timed_out = true;

                        _timeout_callback(client_info.hash);
                    }
                }

                if(m_server_clients.size() > 0 && total_wait_time / m_server_clients.size() > 50)
                    std::cout << "Average: " << total_wait_time / m_server_clients.size() << std::endl;
            }
            else
            {
                auto time_from_last_receive_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_server_receive_timestamp).count();
                auto time_from_last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
                auto time_elapsed_for_timeout_ms = time_from_last_receive_ms - time_from_last_update_ms;

                if (time_elapsed_for_timeout_ms > m_timeout_ms)
                {
                    _timeout_callback(std::nullopt);
                }
            }
        }

        /*
        * Send a message to:
        *   1. Connected server if no client hash is specified (left at 0)
        *   2. At the specified client by the given hash (if registered)
        * Returns if the operation succeeded
        */
        bool Send(const void* _msg, int _msg_size, ClientHash _client_hash = 0) const
        {
            if (m_is_server)
            {
                assert(_client_hash != 0);

                auto client_iter = m_server_clients.find(_client_hash);
                if (client_iter != m_server_clients.end())
                {
                    long total = ikcp_send(client_iter->second.kcp_instance, reinterpret_cast<const char*>(_msg), _msg_size);
                    if (total == 0)
                    {
                        if (IsPingDatagram(reinterpret_cast<const char*>(_msg), _msg_size))
                        {
                            // std::cout << "Connection -> Sent ping! {" << client_iter->second.address << ", " << client_iter->second.port << "}" << std::endl;
                        }
                        else
                        {
                            // std::cout << "Connection -> Sent datagram! {" << client_iter->second.address << ", " << client_iter->second.port << "}" << std::endl;
                        }
                        return true;
                    }
                    else
                    {
                        std::cout << "Connection -> Failed to send datagram! {" << client_iter->second.address << ", " << client_iter->second.port << "}" << std::endl;
                    }
                }
            }
            else
            {
                assert(_client_hash == 0);
                long total = ikcp_send(m_single_kcp_instance, reinterpret_cast<const char*>(_msg), _msg_size);
                if (total == 0)
                {
                    if (IsPingDatagram(reinterpret_cast<const char*>(_msg), _msg_size))
                    {
                        // std::cout << "Connection -> Sent ping! {" << m_dst_address << ", " << m_dst_port << "}" << std::endl;
                    }
                    else
                    {
                        // std::cout << "Connection -> Sent datagram! {" << m_dst_address << ", " << m_dst_port << "}" << std::endl;
                    }
                    return true;
                }
                else
                {
                    std::cout << "Connection -> Failed to send datagram! {" << m_dst_address << ", " << m_dst_port << "}" << std::endl;
                }
            }

            return false;
        }

        /*
        * Receive data from:
        *   1. The connected server, where the callback function will not have valid a client hash parameter
        *   2. From each client that had sent data, where the callback function will have the respective client hash
        */
        void Receive(const ReceiveCallback& _receive_callback) const
        {
            int  buffer_size = MaximumDatagramSize;
            char buffer[MaximumDatagramSize];

            if (m_is_server)
            {
                // For each registered client
                for (auto& [client_hash, client_info] : m_server_clients)
                {
                    while (true)
                    {
                        long total_received = ikcp_recv(client_info.kcp_instance, buffer, buffer_size);
                        if (total_received <= 0)
                        {
                            break;
                        }

                        // [[likely]]
                        if (!IsPingDatagram(buffer, total_received))
                        {
                            // std::cout << "Connection -> Received datagram! {" << client_info.address << ", " << client_info.port << "}" << std::endl;

                            _receive_callback(client_info.hash, nonstd::span<char>(buffer, buffer + total_received));
                        }
                        else
                        {
                            // std::cout << "Connection -> Received ping! {" << client_info.address << ", " << client_info.port << "}" << std::endl;
                        }
                    }
                }
            }
            else
            {
                while (true)
                {
                    long total_received = ikcp_recv(m_single_kcp_instance, buffer, buffer_size);
                    if (total_received <= 0)
                    {
                        break;
                    }

                    // [[likely]]
                    if (!IsPingDatagram(buffer, total_received))
                    {
                        // std::cout << "Connection -> Received datagram! {" << m_dst_address << ", " << m_dst_port << "}" << std::endl;

                        _receive_callback(std::nullopt, nonstd::span<char>(buffer, buffer + total_received));
                    }
                    else
                    {
                        // std::cout << "Connection -> Received ping! {" << m_dst_address << ", " << m_dst_port << "}" << std::endl;
                    }
                }
            }
        }

        /*
        * Returns if this connection is operating as a server
        */
        bool IsServer() const
        {
            return m_is_server;
        }

    private:

        /*
        * Check if the underlying socket has some data to be received
        */
        bool CanReceiveDatagram() const
        {
            fd_set          sready;
            struct timeval  nowait;
            FD_ZERO(&sready);
            FD_SET(m_socket, &sready);
            memset((char*)&nowait, 0, sizeof(nowait));

            bool res = select(m_socket, &sready, NULL, NULL, &nowait);
            if (FD_ISSET(m_socket, &sready))
                res = true;
            else
                res = false;

            return res;
        }

        /*
        * Receive datagrams from the UDP layer and pass them to the kcp
        */
        void TryReceiveDatagrams()
        {
            struct sockaddr_in sender;
            socklen_t          sendersize = sizeof(sender);
            int                buffer_size = MaximumDatagramSize;
            char               buffer[MaximumDatagramSize];

            while (CanReceiveDatagram())
            {
                int total_received = recvfrom(m_socket, buffer, buffer_size, 0, reinterpret_cast<struct sockaddr*>(&sender), &sendersize);
                // [[unlikely]]
                if (total_received <= 0)
                {
                    break;
                }

                if (m_is_server)
                {
                    ClientHash client_hash = HashClientAddr(sender);
                    auto client_iter = m_server_clients.find(client_hash);

                    // [[unlikely]]
                    if (client_iter == m_server_clients.end() || client_iter->second.timed_out)
                    {
                        // Create a client entry
                        ClientInfo new_client;
                        client_iter = m_server_clients.insert({ client_hash, std::move(new_client) }).first;

                        ClientInfo& client_info = client_iter->second;
                        client_info.hash = client_hash;
                        client_info.kcp_instance = ikcp_create(0, &client_info);
                        client_info.address = inet_ntoa(sender.sin_addr);
                        client_info.port = ntohs(sender.sin_port);
                        if (!client_info.kcp_instance)
                        {
                            continue;
                        }

                        ikcp_nodelay(client_info.kcp_instance, 1, IntervalUpdateTime, 2, 1);

                        struct sockaddr_in outaddr;
                        memset(&outaddr, 0, sizeof(outaddr));
                        outaddr.sin_family = AF_INET;
                        outaddr.sin_addr.s_addr = inet_addr(client_info.address.c_str());
                        outaddr.sin_port = htons(client_info.port);

                        client_info.client_addr = std::move(outaddr);
                        client_info.socket = &m_socket;

                        ikcp_setoutput(
                            client_info.kcp_instance,
                            [](const char* buf, int len, ikcpcb* kcp, void* user) -> int
                            {
                                ClientInfo& client_info = *(ClientInfo*)user;

                                return sendto(*client_info.socket, buf, len, 0, (struct sockaddr*) & client_info.client_addr, sizeof(client_info.client_addr));
                            });

                    }

                    client_iter->second.last_receive_timestamp = std::chrono::steady_clock::now();

                    int kcp_result = ikcp_input(client_iter->second.kcp_instance, buffer, static_cast<long>(total_received));
                    // TODO: Do something with kcp_result?               
                }
                else
                {
                    m_last_server_receive_timestamp = std::chrono::steady_clock::now();
                    m_is_waiting_for_ping = false;

                    int kcp_result = ikcp_input(m_single_kcp_instance, buffer, static_cast<long>(total_received));
                    // TODO: Do something with kcp_result?         
                }
            }
        }

        /*
        * Create and configure the listen part of this connection socket
        */
        bool SetupListenSocket()
        {
            //set up bind address
            sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = htonl(INADDR_ANY);
            addr.sin_port = htons(m_local_port);

#ifdef _WIN32
            bool bOptVal = 1;
            int bOptLen = sizeof(bool);
#else
            int OptVal = 1;
#endif
            long retval = 0;

            if (false)
            {
#ifdef _WIN32
                retval = setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, bOptLen);
#else
                retval = setsockopt(m_socket, SOL_SOCKET, SO_BROADCAST, &OptVal, sizeof(OptVal));
#endif
                if (retval != 0)
                {
                    return false;
                }
            }

            if (true)
            {
#ifdef _WIN32
                retval = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, (char*)&bOptVal, bOptLen);
#else
                retval = setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &OptVal, sizeof(OptVal));
#endif
                if (retval != 0)
                {
                    return false;
                }
            }

            if (true)
            {
#ifdef _WIN32
                struct timeval read_timeout;
                read_timeout.tv_sec = 1;
                read_timeout.tv_usec = 1;
                // retval = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&read_timeout, sizeof(read_timeout));
#else
                struct timeval read_timeout;
                read_timeout.tv_sec = 0;
                read_timeout.tv_usec = 1;
                // retval = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
#endif
                if (retval != 0)
                {
                    return false;
                }
            }

            retval = bind(m_socket, (struct sockaddr*) & addr, sizeof(addr));

            if (retval != 0)
            {
                return false;
            }

            return true;
        }

        /*
        * Hash a client addr (address and port)
        */
        ClientHash HashClientAddr(const struct sockaddr_in& _client_addr) const
        {
            return static_cast<ClientHash>(_client_addr.sin_addr.S_un.S_addr ^ _client_addr.sin_port | _client_addr.sin_port >> 15 >> 1);
        }

        /*
        * Check if a given message has a ping encoded
        */
        bool IsPingDatagram(const char* _message, uint32_t _message_size) const
        {
            if (_message_size >= 5
                && _message[0] == 4
                && _message[1] == 28
                && _message[2] == 36
                && _message[3] == 19
                && _message[4] == 111)
            {
                return true;
            }

            return false;
        }

        /*
        * Encode a ping datagram that can be sent over network
        */
        const char* GetPingDatagram(uint32_t& _size) const
        {
            static const char ping_datagram[5] = { 4, 28, 36, 19, 111 };
            _size = sizeof(ping_datagram);
            return ping_datagram;
        }

    private:

#ifdef _WIN32
        WSAData     m_wsa_data;
        SocketType  m_socket          = {};
#else
        SocketType  m_socket          = 0;
#endif
        sockaddr_in m_server_addr     = {};
        uint32_t    m_local_port      = 0;
        uint32_t    m_dst_port        = 0;
        std::string m_dst_address;
        ikcpcb* m_single_kcp_instance = nullptr;

        std::optional<ServerInfo> m_server_info;

        bool        m_is_server           = false;
        bool        m_is_valid            = false;
        bool        m_is_waiting_for_ping = false;

        uint32_t    m_timeout_ms     = 500;
        uint32_t    m_ping_window_ms = 100;

        std::chrono::time_point<std::chrono::steady_clock> m_initial_timestamp             = std::chrono::steady_clock::now();
        std::chrono::time_point<std::chrono::steady_clock> m_last_update_timestamp         = std::chrono::steady_clock::now();
        std::chrono::time_point<std::chrono::steady_clock> m_last_server_receive_timestamp = std::chrono::steady_clock::now();

        std::map<ClientHash, ClientInfo> m_server_clients;
    };

    enum class RequestType : uint64_t
    {
        /* Worker Requests */
        WorkerAuthentication,           /* pure worker type */
        ClientWorkerAuthentication,     /* client worker type */
        WorkerLogMessage, 
        WorkerReserveEntityIdRange, 
        WorkerAddEntity, 
        WorkerRemoveEntity, 
        WorkerAddComponent, 
        WorkerRemoveComponent, 
        WorkerComponentUpdate,
        WorkerComponentQuery, 

        /* Worker Spawner Requests */
        SpawnWorkerForLayer,

        /* Runtime Requests */
    };

    class Request
    {
    public:

        using RequestIndex = uint32_t;

        RequestType  type;
        RequestIndex request_index = std::numeric_limits<RequestIndex>::max();

        friend std::ostream& operator<< (std::ostream& _stream, const Request& _request)
        {
            _stream.write((char*)&_request.type, sizeof(_request.type));
            _stream.write((char*)&_request.request_index, sizeof(_request.request_index));

            return _stream;
        }

        friend std::istream& operator>> (std::istream& _stream, Request& _request)
        {
            _stream.read((char*)&_request.type, sizeof(_request.type));
            _stream.read((char*)&_request.request_index, sizeof(_request.request_index));

            return _stream;
        }
    };

    struct RequestResponse
    {
        friend RequestMaker;
        friend RequestManager;

    protected:
        RequestResponse(cereal::BinaryInputArchive& _archive, const Request& _original_request)
            : original_request(_original_request)
            , archive(_archive)
        {
        }

    public:

        const Request& original_request;

        template<typename ResponsePayloadType>
        ResponsePayloadType GetResponse() const
        {
            ResponsePayloadType temp_response_object;

            {
                archive(temp_response_object);
            }

            return std::move(temp_response_object);
        }

    private:

        cereal::BinaryInputArchive& archive;
    };

    class RequestMaker
    {
        template<typename CharT, typename TraitsT = std::char_traits<CharT> >
        class vectorwrapbuf : public std::basic_streambuf<CharT, TraitsT>
        {
            using Base = std::basic_streambuf<CharT, TraitsT>;

        public:

            vectorwrapbuf(std::vector<CharT>& vec)
            {
                this->setg(vec.data(), vec.data(), vec.data() + vec.size());
                this->setp(vec.data(), vec.data() + vec.size());
            }
            vectorwrapbuf(nonstd::span<CharT>& vec)
            {
                this->setg(vec.data(), vec.data(), vec.data() + vec.size());
                this->setp(vec.data(), vec.data() + vec.size());
            }

            Base::pos_type seekoff(
                Base::off_type off,
                std::ios_base::seekdir dir,
                std::ios_base::openmode which = std::ios_base::in) override
            {
                if (dir == std::ios_base::cur)
                    Base::gbump(off);
                else if (dir == std::ios_base::end)
                    Base::setg(Base::eback(), Base::egptr() + off, Base::egptr());
                else if (dir == std::ios_base::beg)
                    Base::setg(Base::eback(), Base::eback() + off, Base::egptr());
                return (which == std::ios_base::in ? Base::gptr() : Base::pptr()) - Base::eback();
            }
        };

    public:

        template<typename RequestPayloadType>
        bool MakeRequest(const Connection<>& _connection, RequestType _request_type, const RequestPayloadType& _payload)
        {
            assert(!_connection.IsServer());

            m_temporary_request_buffer.clear();
            m_temporary_request_buffer.resize(sizeof(Request) + sizeof(RequestPayloadType));

            vectorwrapbuf<char> data_buffer(m_temporary_request_buffer);
            std::ostream        out_stream(&data_buffer);

            Request new_request;
            new_request.type = _request_type;
            new_request.request_index = m_request_counter++;

            out_stream << new_request;

            {
                cereal::BinaryOutputArchive archive(out_stream);
                archive(_payload);
            }

            return _connection.Send(m_temporary_request_buffer.data(), out_stream.tellp());
        }

        void CheckResponses(const Connection<>& _connection, std::function<void(const Request&, const RequestResponse&)> _response)
        {
            assert(!_connection.IsServer());

            _connection.Receive(
                [&](auto _client_hash, nonstd::span<char> _data)
                {
                    if (_data.size() < sizeof(Request))
                    {
                        std::cout << "RequestMaker -> Received invalid data, ignoring!" << std::endl;
                        return;
                    }

                    vectorwrapbuf<char> data_buffer(_data);
                    std::istream        in_stream(&data_buffer);
                    cereal::BinaryInputArchive archive(in_stream);

                    Request original_request;
                    in_stream >> original_request;

                    RequestResponse request_response(archive, original_request);

                    _response(original_request, request_response);
                });
        }

    private:

        Request::RequestIndex m_request_counter = 0;
        std::vector<char>     m_temporary_request_buffer;
    };

    class RequestManager
    {
        template<typename CharT, typename TraitsT = std::char_traits<CharT> >
        class vectorwrapbuf : public std::basic_streambuf<CharT, TraitsT>
        {
            using Base = std::basic_streambuf<CharT, TraitsT>;

        public:

            vectorwrapbuf(std::vector<CharT>& vec)
            {
                this->setg(vec.data(), vec.data(), vec.data() + vec.size());
                this->setp(vec.data(), vec.data() + vec.size());
            }
            vectorwrapbuf(nonstd::span<CharT>& vec)
            {
                this->setg(vec.data(), vec.data(), vec.data() + vec.size());
                this->setp(vec.data(), vec.data() + vec.size());
            }

            Base::pos_type seekoff(
                Base::off_type off,
                std::ios_base::seekdir dir,
                std::ios_base::openmode which = std::ios_base::in) override
            {
                if (dir == std::ios_base::cur)
                    Base::gbump(off);
                else if (dir == std::ios_base::end)
                    Base::setg(Base::eback(), Base::egptr() + off, Base::egptr());
                else if (dir == std::ios_base::beg)
                    Base::setg(Base::eback(), Base::eback() + off, Base::egptr());
                return (which == std::ios_base::in ? Base::gptr() : Base::pptr()) - Base::eback();
            }
        };

    public:

        void CheckRequests(const Connection<>& _connection, std::function<void(Connection<>::ClientHash, const Request&, cereal::BinaryInputArchive&, cereal::BinaryOutputArchive&)> _response)
        {
            assert(_connection.IsServer());

            _connection.Receive(
                [&](auto _client_hash, nonstd::span<char> _data)
                {
                    if (_data.size() < sizeof(Request) || !_client_hash)
                    {
                        // Problem!
                        std::cout << "RequestManager -> Received invalid data, ignoring!" << std::endl;
                        return;
                    }

                    m_temporary_request_buffer.clear();
                    m_temporary_request_buffer.resize(4096);

                    vectorwrapbuf<char> in_data_buffer(_data);
                    std::istream        in_stream(&in_data_buffer);
                    cereal::BinaryInputArchive in_archive(in_stream);

                    vectorwrapbuf<char> out_data_buffer(m_temporary_request_buffer);
                    std::ostream        out_stream(&out_data_buffer);
                    cereal::BinaryOutputArchive out_archive(out_stream);

                    Request original_request;
                    in_stream >> original_request;

                    out_stream << original_request;

                    _response(_client_hash.value(), original_request, in_archive, out_archive);

                    _connection.Send(m_temporary_request_buffer.data(), out_stream.tellp(), _client_hash.value());
                });
        }

        std::vector<char> m_temporary_request_buffer;
    };

} // namespace Jani