////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdint>
#include <string>
#include <optional>
#include <filesystem>
#include <functional>
#include <iostream>
#include <fstream>
#include <set>
#include <thread>
#include <limits>
#include <mutex>
#include <string>
#include <variant>
#include <bitset>
#include <entityx/entityx.h>
#include <boost/pfr.hpp>
#include <magic_enum.hpp>
#include <ctti/type_id.hpp>
#include <ctti/static_value.hpp>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <nlohmann/json.hpp>
#include "span.hpp"

#include <ikcp.h>

//
//
//

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

//
//
//

#undef max
#undef min

/////////////
// DEFINES //
/////////////

#define JaniNamespaceBegin(name)                  namespace name {
#define JaniNamespaceEnd(name)                    }

JaniNamespaceBegin(Jani)

class Bridge;
class Runtime;
class Database;

struct WorldPosition
{
    int32_t x, y;
};

struct WorldArea
{
    uint32_t width, height;
};

struct WorldRect
{
    uint32_t ManhattanDistanceFromPosition(WorldPosition _position) const
    {
        int32_t center_x  = width - x;
        int32_t center_y  = height - y;
        uint32_t distance = std::abs(center_x - _position.x) + std::abs(center_y - _position.y);
        uint32_t max_side = std::max(width, height);

        return static_cast<int32_t>(distance) - static_cast<int32_t>(max_side / 2) < 0 ? 0 : distance - max_side / 2;
    }

    int32_t AreaDifference(const WorldRect& _other_rect) const
    {
        return width * height - _other_rect.width * _other_rect.height;
    }

    int32_t x, y;
    uint32_t width, height;
};

static void to_json(nlohmann::json& j, const Jani::WorldPosition& _object)
{
    j = nlohmann::json
    {
        {"x", _object.x},
        {"y", _object.y}
    };
}

static void from_json(const nlohmann::json& j, Jani::WorldPosition& _object)
{
    j.at("x").get_to(_object.x);
    j.at("y").get_to(_object.y);
}

static void to_json(nlohmann::json& j, const Jani::WorldArea& _object)
{
    j = nlohmann::json
    {
        {"width", _object.width},
        {"height", _object.height}
    };
}

static void from_json(const nlohmann::json& j, Jani::WorldArea& _object)
{
    j.at("width").get_to(_object.width);
    j.at("height").get_to(_object.height);
}

static void to_json(nlohmann::json& j, const Jani::WorldRect& _object)
{
    j = nlohmann::json
    {
        {"x", _object.x},
        {"y", _object.y},
        {"width", _object.width},
        {"height", _object.height}
    };
}

static void from_json(const nlohmann::json& j, Jani::WorldRect& _object)
{
    j.at("x").get_to(_object.x);
    j.at("y").get_to(_object.y);
    j.at("width").get_to(_object.width);
    j.at("height").get_to(_object.height);
}

class User
{
public:

    /*
    
    */
    WorldPosition GetRepresentativeWorldPosition() const;

private:

    WorldPosition user_world_position;
};

using WorkerId = uint64_t;

using ComponentId = uint64_t;

using EntityId = uint64_t;

enum class WorkerLogLevel
{
    Trace,
    Info,
    Warning,
    Error,
    Critical
};

struct WorkerRequestResult
{
    explicit WorkerRequestResult(bool _succeed) : succeed(_succeed) {}
    WorkerRequestResult(bool _succeed, std::vector<int8_t> _payload) : succeed(_succeed), payload(std::move(_payload)) {}
    WorkerRequestResult(bool _succeed, int8_t* _payload_data, uint32_t _payload_size) : succeed(_succeed), payload(std::vector<int8_t>(_payload_data, _payload_data + _payload_size)) {}

    bool                succeed = false;
    std::vector<int8_t> payload;
};



/*
* 1. Entity who owns the component
* 2. The component id
* 3. The worker who has the interest in receiving component updates
*/
using ComponentUpdateReadInterest = std::tuple<EntityId, ComponentId, WorkerId>;

struct ComponentPayload
{
    ComponentId         component_id;
    std::vector<int8_t> component_data;
};

struct EntityPayload
{
    std::vector<ComponentPayload> component_payloads;
};

static const uint32_t MaximumEntityComponents = 64;
using ComponentMask = std::bitset<MaximumEntityComponents>;

class Entity
{
public:

    Entity(EntityId _unique_id) : entity_id(_unique_id) {};

    /*

    */
    WorldPosition GetRepresentativeWorldPosition() const
    {
        return world_position;
    }

    /*
    
    */
    void SetRepresentativeWorldPosition(WorldPosition _position)
    {
        world_position = _position;
    }

    /*
    
    */
    EntityId GetUniqueId() const
    {
        return entity_id;
    }

    /*
    
    */
    bool HasComponent(ComponentId _component_id) const
    {
        return component_mask.test(_component_id);
    }
    bool HasComponents(ComponentMask _component_mask) const
    {
        for (int i = 0; i < MaximumEntityComponents; i++)
        {
            if (component_mask[i] && !_component_mask[i])
            {
                return false;
            }
        }

        return true;
    }

    void AddComponent(ComponentId _component_id)
    {
        component_mask[_component_id] = true;
    }

    void RemoveComponent(ComponentId _component_id)
    {
        component_mask[_component_id] = false;
    }

private:

    EntityId      entity_id      = std::numeric_limits<EntityId>::max();
    WorldPosition world_position = { 0, 0 };
    ComponentMask component_mask;
};

template <typename EnumType, typename EnumBitsType = uint32_t>
class enum_bitset
{
public:

    enum_bitset()  = default;
    ~enum_bitset() = default;

    explicit enum_bitset(EnumType _enum_value) : m_bitset(static_cast<EnumBitsType>(_enum_value))
    {
    }

    bool operator ==(EnumBitsType _bits) const
    {
        return m_bitset == static_cast<EnumBitsType>(_bits);
    }

    template <typename OtherEnumType, typename OtherEnumBitsType>
    bool operator ==(const enum_bitset<OtherEnumType, OtherEnumBitsType>& _bits) const
    {
        return m_bitset == _bits.get_raw();
    }

    bool operator !=(EnumBitsType _bits) const
    {
        return !(*this == _bits);
    }

    template <typename OtherEnumType, typename OtherEnumBitsType>
    bool operator !=(const enum_bitset<OtherEnumType, OtherEnumBitsType>& _bits) const
    {
        return !(*this == _bits);
    }

    enum_bitset operator <<(EnumType _enum)
    {
        m_bitset = m_bitset | static_cast<EnumBitsType>(_enum);
        return *this;
    }

    enum_bitset operator <<(EnumBitsType _bits)
    {
        m_bitset = m_bitset | _bits;
        return *this;
    }

    bool operator &(EnumType _bits) const
    {
        return m_bitset & static_cast<EnumBitsType>(_bits);
    }

    operator EnumType() const
    {
        return static_cast<EnumType>(m_bitset);
    }

    EnumBitsType get_raw() const
    {
        return m_bitset;
    }

private:

    EnumBitsType m_bitset = 0;
};

enum class LayerLoadBalanceStrategyBits
{
    None           = 0,
    MaximumWorkers = 1 << 0, 
    SpatialArea    = 1 << 1
};

using LayerLoadBalanceStrategyTypeFlags = enum_bitset<LayerLoadBalanceStrategyBits>;

struct LayerLoadBalanceStrategy
{
    std::optional<uint32_t>  maximum_workers;
    std::optional<WorldArea> minimum_area;
    std::optional<WorldArea> maximum_area;
};

static const uint32_t MaximumPacketSize = 4096;

#if 0

//Simple socket class for datagrams.  Platform independent between
//unix and Windows.
class DatagramSocket
{
private:
#ifdef _WIN32
    WSAData wsaData;
    SOCKET sock;
#else
    int sock;
#endif
    long retval;
    sockaddr_in outaddr;
    char ip[30];
    char received[30];

    uint32_t receive_port;
    uint32_t dst_port;

public:
    DatagramSocket(int _receive_port, int _dst_port, const char* _dst_address, bool _broadcast, bool _reuse_socket);
    ~DatagramSocket();

    bool CanReceive();
    long Receive(char* msg, int msgsize);
    char* ReceivedFrom();
    long Send(const char* msg, int msgsize) const;
    long SendTo(const char* msg, int msgsize, const char* name);
    int GetAddress(const char* name, char* addr);
    const char* GetAddress(const char* name);

    uint32_t GetReceivePort() const;
    uint32_t GetDstPort() const;
};

//
//
//



//
//
//

class Connection
{
public:

    static const uint32_t MaximumDatagramSize = 2048;

    Connection(uint32_t _instance_id, int _receive_port, int _dst_port, const char* _dst_address, std::optional<uint32_t> _heartbeat_ms = std::nullopt);
    ~Connection();

    void Update();

    /*
    * Set the minimum time required to detect a timeout on this connection
    * The value specified should be in ms
    */
    void SetTimeoutRequired(uint32_t _timeout_ms);

    /*
    * Returns if this connection timed-out
    */
    bool DidTimeout() const;

    template <typename ByteType>
    std::optional<size_t> Receive(ByteType* _msg, int _buffer_size)
    {
        std::lock_guard l(m_safety_mtx);

        long total   = ikcp_recv(m_internal_connection, reinterpret_cast<char*>(_msg), _buffer_size);
        bool is_ping = IsPingDatagram(reinterpret_cast<char*>(_msg), _buffer_size);
        if (total >= 0 && !is_ping)
        {
            return static_cast<size_t>(total);
        }

        if (is_ping)
        {
            std::cout << "Received ping" << std::endl;
        }

        return std::nullopt;
    }

    template <typename ByteType>
    std::optional<size_t> Send(const ByteType* _msg, int _msg_size) const
    {
        std::lock_guard l(m_safety_mtx);

        long total = ikcp_send(m_internal_connection, reinterpret_cast<const char*>(_msg), _msg_size);
        if (total == 0)
        {
            return static_cast<size_t>(_msg_size);
        }

        return std::nullopt;
    }

    uint32_t GetReceiverPort()    const;
    uint32_t GetDestinationPort() const;

private:



private:

    std::unique_ptr<DatagramSocket>                    m_datagram_socket;
    std::optional<uint32_t>                            m_heartbeat_ms;
    std::chrono::time_point<std::chrono::steady_clock> m_initial_timestamp      = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> m_last_update_timestamp  = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> m_last_receive_timestamp = std::chrono::steady_clock::now();
    ikcpcb*                                            m_internal_connection    = nullptr;
    std::thread                                        m_update_thread;
    bool                                               m_exit_update_thread     = false;
    bool                                               m_is_waiting_for_ping    = false;
    uint32_t                                           m_timeout_amount_ms      = 500;
    mutable std::mutex                                 m_safety_mtx;
};

class ConnectionListener
{
public:
    static const uint32_t MaximumDatagramSize = 2048;

    ConnectionListener(int _listen_port);
    ~ConnectionListener();

    template <typename ByteType>
    std::optional<size_t> Receive(ByteType* _msg, int _buffer_size)
    {
        if (m_datagram_socket->CanReceive())
        {
            long total = m_datagram_socket->Receive(reinterpret_cast<char*>(_msg), _buffer_size);
            if (total > 0)
            {
                return static_cast<size_t>(total);
            }
        }

        return std::nullopt;
    }

    uint32_t GetReceiverPort() const
    {
        return m_datagram_socket->GetReceivePort();
    }

private:

    std::unique_ptr<DatagramSocket> m_datagram_socket;
};

class ConnectionRequest
{
public:

    ConnectionRequest(int _dst_port, const char* _dst_address) :
        m_socket(0, _dst_port, _dst_address, false, false)
    {}

    template <typename ByteType>
    std::optional<size_t> Send(const ByteType* _msg, int _msg_size) const
    {
        long total_sent = m_socket.Send(reinterpret_cast<const char*>(_msg), _msg_size);
        if (total_sent <= 0)
        {
            return std::nullopt;
        }

        return total_sent;
    }

private:

    DatagramSocket m_socket;
};

#endif

#if 0
using AuthenticationStructType = void*;
using ClientHashType = uint64_t;
static uint32_t MaximumDatagramSize = 2048;
#endif

template <typename AuthenticationStructType = void*, typename ClientHashType = uint64_t, uint32_t MaximumDatagramSize = 2048>
class Connection
{
public:

    using ClientHash               = ClientHashType;
    using ReceiveCallback          = std::function<void(std::optional<ClientHash>, nonstd::span<char>)>;
    using TimeoutCallback          = std::function<void(std::optional<ClientHash>)>;
    using AuthenticationCallback   = std::function<bool(ClientHash, const AuthenticationStructType&)>;

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
        ClientHash                                         hash                   = std::numeric_limits<ClientHash>::max();
        ikcpcb*                                            kcp_instance           = nullptr;
        std::chrono::time_point<std::chrono::steady_clock> last_receive_timestamp = std::chrono::steady_clock::now();
        std::string                                        address;
        uint16_t                                           port                   = std::numeric_limits<uint16_t>::max();
        mutable bool                                       authenticated          = false;
        mutable bool                                       timed_out              = false;
        SocketType*                                        socket                 = nullptr;
        struct sockaddr_in                                 client_addr;
    };

    struct ServerInfo
    {
        struct sockaddr_in server_addr;
        SocketType*        socket = nullptr;
    };

public:

    /*
    * Setup a client connection type
    * This connection will only be allowed to send data to the dst address/port
    * This connection will only be allowed to receive data from the dst address/port
    */
    Connection(int _local_port, int _dst_port, const char* _dst_address)
    {
        m_is_server   = false;
        m_local_port  = _local_port;
        m_dst_port    = _dst_port;
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

        {
            struct sockaddr_in outaddr;
            memset(&outaddr, 0, sizeof(outaddr));
            outaddr.sin_family      = AF_INET;
            outaddr.sin_addr.s_addr = inet_addr(m_dst_address.c_str());
            outaddr.sin_port        = htons(m_dst_port);

            m_server_info = { std::move(outaddr), &m_socket };
        }

        ikcp_setoutput(
            m_single_kcp_instance,
            [](const char* buf, int len, ikcpcb* kcp, void* user) -> int
            {
                ServerInfo& server_info = *(ServerInfo*)user;

                return sendto(*server_info.socket, buf, len, 0, (struct sockaddr*)& server_info.server_addr, sizeof(server_info.server_addr));
            });

        m_is_valid = true;
    }

    /*
    * Setup a server connection type
    * This connection will be allowed to send data to any registered and authenticated
    * client
    * This connection will be able to receive data from any client that knows its
    * address/port
    */
    Connection(int _local_port, std::optional<AuthenticationCallback> _authentication_callback)
    {
        m_is_server   = true;
        m_local_port  = _local_port;

        if (m_local_port == 0)
        {
            return;
        }

        if (_authentication_callback)
        {
            m_client_authentication_callback = _authentication_callback.value();
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

    /*
    * This function will attempt to grab any received datagram from the UDP layer and
    * pass it to the corresponding kcp instance
    * If this is a client, it will also check if a ping is required to keep the 
    * connection alive
    */
    void Update()
    {
        auto time_now     = std::chrono::steady_clock::now();
        auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_initial_timestamp).count();

        TryReceiveDatagrams();

        // Update the kcp instance(s)
        if (m_is_server)
        {
            // For each registered client
            for (auto& [client_hash, client_info] : m_server_clients)
            {
                auto target_update_time        = ikcp_check(client_info.kcp_instance, time_elapsed);
                auto time_remaining_for_update = target_update_time - time_elapsed;

                if (time_remaining_for_update <= 0)
                {
                    ikcp_update(client_info.kcp_instance, time_elapsed);
                }
            }
        }
        else
        {
            auto target_update_time        = ikcp_check(m_single_kcp_instance, time_elapsed);
            auto time_remaining_for_update = target_update_time - time_elapsed;

            if (time_remaining_for_update <= 0)
            {
                ikcp_update(m_single_kcp_instance, time_elapsed);
            }
        }

        // It's client job to ping the server and not the opposite
        if (!m_is_server && !m_is_waiting_for_ping)
        {
            auto time_now                  = std::chrono::steady_clock::now();
            auto time_from_last_receive_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_server_receive_timestamp).count();
            auto time_from_last_update_ms  = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
            auto time_elapsed_for_ping_ms  = time_from_last_receive_ms - time_from_last_update_ms;

            if (time_elapsed_for_ping_ms > m_ping_window_ms)
            {
                uint32_t ping_datagram_size = 0;
                auto*    ping_datagram      = GetPingDatagram(ping_datagram_size);
                m_is_waiting_for_ping       = Send(ping_datagram, ping_datagram_size);
            }
        }

        m_last_update_timestamp = std::chrono::steady_clock::now();
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
            for (auto& [client_hash, client_info] : m_server_clients)
            {
                auto time_from_last_receive_ms   = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - client_info.last_receive_timestamp).count();
                auto time_from_last_update_ms    = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
                auto time_elapsed_for_timeout_ms = time_from_last_receive_ms - time_from_last_update_ms;

                if (time_elapsed_for_timeout_ms > m_timeout_ms)
                {
                    client_info.timed_out = true;

                    _timeout_callback(client_info.hash);
                }
            }
        }
        else
        {
            auto time_from_last_receive_ms   = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_server_receive_timestamp).count();
            auto time_from_last_update_ms    = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
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
                    return true;
                }
            }
        }
        else
        {
            assert(_client_hash == 0);
            long total = ikcp_send(m_single_kcp_instance, reinterpret_cast<const char*>(_msg), _msg_size);
            if (total == 0)
            {
                return true;
            }
        }

        return false;
    }

    /*
    * Receive data from:
    *   1. The connected server, where the callback function will not have valid a client hash parameter
    *   2. From each client that had sent data, where the callback function will have the respective client hash
    * If this is a server and client authentication is enabled, this function has the potential of triggering the
    * authentication callback whenever a client successfully send its first message
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
                while (long total_received = ikcp_recv(client_info.kcp_instance, buffer, buffer_size) > 0)
                {
                    // [[likely]]
                    if (!IsPingDatagram(buffer, total_received))
                    {
                        // If this server connection requires authentication and this user isn't authenticated, 
                        // proceed with the authentication callback as the first message sent by the user should
                        // be the authentication token (else ignore until it sends the right message or time-out)
                        // [[unlikely]]
                        if (!client_info.authenticated && m_client_authentication_callback)
                        {
                            if (total_received == sizeof(AuthenticationStructType))
                            {
                                client_info.authenticated = m_client_authentication_callback(
                                    client_info.hash,
                                    *reinterpret_cast<AuthenticationStructType*>(buffer));
                            }
                        }
                        else
                        {
                            _receive_callback(client_info.hash, nonstd::span<char>(buffer, buffer + total_received));
                        }
                    }
                }
            }
        }
        else
        {
            while (long total_received = ikcp_recv(m_single_kcp_instance, buffer, buffer_size) > 0)
            {
                // [[likely]]
                if (!IsPingDatagram(buffer, buffer_size))
                {
                    _receive_callback(std::nullopt, nonstd::span<char>(buffer, buffer + total_received));
                }
            }
        }
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
        socklen_t          sendersize  = sizeof(sender);
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
                auto client_iter       = m_server_clients.find(client_hash);

                // [[unlikely]]
                if (client_iter == m_server_clients.end() || client_iter->second.timed_out)
                {
                    // Create a client entry
                    ClientInfo new_client;
                    client_iter = m_server_clients.insert({ client_hash, std::move(new_client) }).first;

                    ClientInfo& client_info  = client_iter->second;
                    client_info.hash         = client_hash;
                    client_info.kcp_instance = ikcp_create(0, &client_info);
                    client_info.address      = inet_ntoa(sender.sin_addr);
                    client_info.port         = sender.sin_port;
                    if (!client_info.kcp_instance)
                    {
                        continue;
                    }
  
                    struct sockaddr_in outaddr;
                    memset(&outaddr, 0, sizeof(outaddr));
                    outaddr.sin_family      = AF_INET;
                    outaddr.sin_addr.s_addr = inet_addr(client_info.address.c_str());
                    outaddr.sin_port        = htons(client_info.port);

                    client_info.client_addr = std::move(outaddr);
                    client_info.socket      = &m_socket;

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
                m_is_waiting_for_ping           = false;

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
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port        = htons(m_local_port);

#ifdef _WIN32
        bool bOptVal = 1;
        int bOptLen  = sizeof(bool);
#else
        int OptVal   = 1;
#endif
        long retval  = 0;

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
            retval = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&read_timeout, sizeof(read_timeout));
#else
            struct timeval read_timeout;
            read_timeout.tv_sec = 0;
            read_timeout.tv_usec = 1;
            retval = setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
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
        return static_cast<ClientHash>(_client_addr.sin_addr.S_un.S_addr ^ _client_addr.sin_port | _client_addr.sin_port >> 16);
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
    SocketType  m_socket              = {};
#else
    SocketType  m_socket              = 0;
#endif
    sockaddr_in m_server_addr         = {};
    uint32_t    m_local_port          = 0;
    uint32_t    m_dst_port            = 0;
    std::string m_dst_address;
    ikcpcb*     m_single_kcp_instance = nullptr;

    std::optional<ServerInfo> m_server_info;

    bool        m_is_server           = false;
    bool        m_is_valid            = false;
    bool        m_is_waiting_for_ping = false;

    uint32_t    m_timeout_ms          = 500;
    uint32_t    m_ping_window_ms      = 100;

    AuthenticationCallback m_client_authentication_callback;

    std::chrono::time_point<std::chrono::steady_clock> m_initial_timestamp             = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> m_last_update_timestamp         = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> m_last_server_receive_timestamp = std::chrono::steady_clock::now();

    std::map<ClientHash, ClientInfo> m_server_clients;
};

class RuntimeInterface
{

};

/*
* Communication between a worker and a bridge
*/
class BridgeInterface
{
public:

    BridgeInterface(int _local_port, int _bridge_port, const char* _bridge_address) :
        m_bridge_connection(_local_port, _bridge_port, _bridge_address),
        m_temporary_data_buffer(std::array<char, MaximumPacketSize>())
    {

    }

    void PoolIncommingData(const Connection<>::ReceiveCallback& _receive_callback)
    {
        m_bridge_connection.Receive(_receive_callback);
    }

    bool PushData(const char* _data, size_t _data_size)
    {
        return m_bridge_connection.Send(_data, _data_size);
    }

private:

    Connection<>                        m_bridge_connection;
    std::array<char, MaximumPacketSize> m_temporary_data_buffer;
};

using Hash = uint64_t;

// A hasher object
struct Hasher
{
private:

	template <typename ResultT, ResultT OffsetBasis, ResultT Prime>
	class basic_fnv1a final
	{
		static_assert(std::is_unsigned<ResultT>::value, "need unsigned integer");
		using result_type = ResultT;
		result_type state_{};

	public:

		constexpr
			basic_fnv1a() noexcept : state_{ OffsetBasis }
		{
		}

		constexpr void
			update(const char *const data, const std::size_t size) noexcept
		{
			auto acc = this->state_;
			for (auto i = std::size_t{}; i < size; ++i)
			{
				const auto next = std::size_t(data[i]);
				acc = ResultT((acc ^ next) * Prime);
			}
			this->state_ = acc;
		}

		constexpr void
			update(const char *const data) noexcept
		{
			auto acc = this->state_;
			for (auto i = std::size_t{}; data[i] != 0; ++i)
			{
				const auto next = std::size_t(data[i]);
				acc = ResultT((acc ^ next) * Prime);
			}
			this->state_ = acc;
		}

		constexpr result_type
			digest() const noexcept
		{
			return this->state_;
		}
	};

	using fnv1a_32 = basic_fnv1a<Hash,
		UINT32_C(2166136261),
		UINT32_C(16777619)>;

	using fnv1a_64 = basic_fnv1a<Hash,
		UINT64_C(14695981039346656037),
		UINT64_C(1099511628211)>;

public:

    Hasher()
    {}
    Hasher(Hash _hash) : h(_hash)
    {}
    Hasher(uint32_t _value)
    {
        u32(_value);
    }
    Hasher(float _value)
    {
        f32(_value);
    }
    Hasher(const void* _ptr)
    {
        pointer(_ptr);
    }
    Hasher(const char* _str)
    {
        string(_str);
    }
    Hasher(const std::string& _str)
    {
        string(_str.c_str());
    }

    inline Hasher& operator()(bool _value)
    {
        u32(static_cast<uint32_t>(_value));
        return *this;
    }

    inline Hasher& operator()(uint32_t _value)
    {
        u32(_value);
        return *this;
    }

    inline Hasher& operator()(int32_t _value)
    {
        u32(static_cast<uint32_t>(_value));
        return *this;
    }

    inline Hasher& operator()(uint64_t _value)
    {
        u64(_value);
        return *this;
    }

    inline Hasher& operator()(int64_t _value)
    {
        u64(static_cast<uint64_t>(_value));
        return *this;
    }

    inline Hasher& operator()(float _value)
    {
        f32(_value);
        return *this;
    }

    inline Hasher& operator()(double _value)
    {
        f64(_value);
        return *this;
    }

    template <typename T>
    inline Hasher& operator()(const T* _data, size_t _size)
    {
        data(_data, _size);
        return *this;
    }

    template <typename T>
    inline Hasher& operator()(T* _data, size_t _size)
    {
        data(_data, _size);
        return *this;
    }

    inline Hasher& operator()(const void* _ptr)
    {
        u64(reinterpret_cast<uintptr_t>(_ptr));
        return *this;
    }

    template <typename ObjectType>
    inline Hasher& operator()(const ObjectType* _ptr)
    {
        pointer(static_cast<const void*>(_ptr));
        return *this;
    }

    inline Hasher& operator()(const char* _str)
    {
#ifndef NDEBUG
        int overflow_counter = 0;
#endif
        char c;
        while ((c = *_str++) != '\0')
        {
            u32(uint8_t(c));
#ifndef NDEBUG
            overflow_counter++;
            if (overflow_counter > 1000000)
            {
                assert(false);
            }
#endif
        }
        return *this;
    }

    inline Hasher& operator()(const std::string& _str)
    {
        string(_str.c_str());
        return *this;
    }

    // Container operators

    template <class _T>
    inline Hasher& operator()(const std::vector<_T>& _vector)
    {
        for (auto& value : _vector)
        {
            this->operator()(value);
        }
        return *this;
    }

    template <class _Key, class _T>
    inline Hasher& operator()(const std::map<_Key, _T>& _map)
    {
        for (auto& [key, value] : _map)
        {
            this->operator()(key);
            this->operator()(value);
        }
        return *this;
    }

    template <class _Key, class _T>
    inline Hasher& operator()(const std::unordered_map<_Key, _T>& _map)
    {
        for (auto& [key, value] : _map)
        {
            this->operator()(key);
            this->operator()(value);
        }
        return *this;
    }

    template <class ... Types>
    inline Hasher& operator()(const std::variant<Types ...>& _variant)
    {
        std::visit([&](auto&& _arg) 
        {
            this->operator()(_arg); 
        }, _variant);
        return *this;
    }

    template <class _T1, class _T2>
    inline Hasher& operator()(const std::pair<_T1, _T2>& _a)
    {
        this->operator()(_a.first);
        this->operator()(_a.second);
        return *this;
    }

    /*
    // Only selects this if n parameters > 1
    template<typename... Types>
    Hasher& operator()(Types&& ... args)
    {
        for_each_arg(args...);
        return *this;
    }
    */

    template <typename T, class = typename std::enable_if< std::is_enum<T>::value >::type>
    inline Hasher& operator()(const T& _value)
    {
        this->operator()(static_cast<int>(_value));
        return *this;
    }

    inline Hash get() const
    {
        return h;
    }

    inline operator Hash() const
    {
        return h;
    }

    struct StructType
    {
        int a;
    };

    template <class _T>
    inline Hasher& struct_visit(const _T& _struct)
    {
        boost::pfr::for_each_field(_struct, [&](auto& _field, std::size_t _idx)
        {
            this->operator()(_field);
        });

        return *this;
    }

    template <class _T>
    inline Hasher& struct_visit_flat(const _T& _struct)
    {
        boost::pfr::flat_for_each_field(_struct, [&](auto& _field, std::size_t _idx)
        {
            this->operator()(_field);
        });

        return *this;
    }

    static Hash default_value()
    {
        return 0xcbf29ce484222325ull;
    }

private:

    template<class...Args>
    void for_each_arg(Args&& ...args) 
    {
        (this->operator()(std::forward<Args>(args)), ...);
    }
                
	static constexpr Hash FNV1A(const char *const _str, const std::size_t _size) noexcept
	{
		fnv1a_64 hashfn;;
		hashfn.update(_str, _size);
		return Hash(hashfn.digest());
	}

	static constexpr Hash FNV1A(const char *const _str) noexcept
	{
		fnv1a_64 hashfn;;
		hashfn.update(_str);
		return Hash(hashfn.digest());
	}

    static constexpr Hash FNV1A(const std::string& _str) noexcept
    {
        fnv1a_64 hashfn;;
        hashfn.update(_str.c_str());
        return Hash(hashfn.digest());
    }
		
	template <typename T>
	inline void data(const T *data, size_t size)
	{
		size /= sizeof(*data);
		for (size_t i = 0; i < size; i++)
			h = (h * 0x100000001b3ull) ^ data[i];
	}

	inline void u32(uint32_t value)
	{
		h = (h * 0x100000001b3ull) ^ value;
	}

	inline void f32(float value)
	{
		union
		{
			float f32;
			uint32_t u32;
		} u;
		u.f32 = value;
		u32(u.u32);
	}

    inline void f64(float value)
    {
        union
        {
            float f64;
            uint32_t u64;
        } u;
        u.f64 = value;
        u64(u.u64);
    }

	inline void u64(uint64_t value)
	{
		u32(value & 0xffffffffu);
		u32(value >> 32);
	}

	inline void hash(Hash value)
	{
		u64(value);
	}

	inline void pointer(const void *ptr)
	{
		u64(reinterpret_cast<uintptr_t>(ptr));
	}

	template <typename ObjectType>
	inline void pointer(const ObjectType* _ptr)
	{
		return pointer(static_cast<const void*>(_ptr));
	}

	inline void string(const char *str)
	{
		char c;
		while ((c = *str++) != '\0')
			u32(uint8_t(c));
	}

    inline void string(const std::string& str)
    {
        string(str.c_str());
    }

private:

	Hash h = default_value();
};

struct ComponentQueryInstruction
{   
    std::vector<ComponentId> component_constraints;
    ComponentMask            component_constraints_mask = 0;

    std::optional<WorldArea> area_constraint;
    std::optional<uint32_t>  radius_constraint;

    // Only one is allowed
    bool and_constraint = false;
    bool or_constraint  = false;

    // If linked with multiple instructions, the frequency from the last one will be
    // used instead
    uint32_t frequency = 1;
};

struct ComponentQuery
{
    friend Runtime;

    ComponentQuery(ComponentId _query_owner_component, std::optional<WorldPosition> _query_position) : 
        query_owner_component(_query_owner_component), 
        query_position(_query_position)
    {}

    ComponentQuery& QueryComponent(ComponentId _component_id)
    {
        query_component_ids.push_back(_component_id);
        return *this;
    }

    ComponentQuery& QueryComponents(std::vector<ComponentId> _component_ids)
    {
        query_component_ids.insert(query_component_ids.end(), _component_ids.begin(), _component_ids.end());
        return *this;
    }

    ComponentQuery& WhereEntityOwnerHasComponent(ComponentId _component_id)
    { 
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        current_query.component_constraints.push_back(_component_id);
        return *this;
    }

    ComponentQuery& WhereEntityOwnerHasComponents(std::vector<ComponentId> _component_ids)
    {
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        current_query.component_constraints.insert(current_query.component_constraints.end(), _component_ids.begin(), _component_ids.end());
        return *this;
    }

    ComponentQuery& InArea(WorldArea _area)
    {
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        assert(!current_query.radius_constraint);
        current_query.area_constraint = _area;
        return *this;
    }

    ComponentQuery& InRadius(uint32_t _radius)
    {
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        assert(!current_query.area_constraint);
        current_query.radius_constraint = _radius;
        return *this;
    }

    ComponentQuery& WithFrequency(uint32_t _frequency)
    {
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        current_query.frequency = _frequency;
        return *this;
    }

    ComponentQuery& And()
    {
        auto& current_query = AddOrGetQuery();

        assert(!current_query.or_constraint);

        current_query.and_constraint = true;
        AddOrGetQuery(true);
        return *this;
    }

    ComponentQuery& Or()
    {
        auto& current_query = AddOrGetQuery();

        assert(!current_query.and_constraint);

        current_query.or_constraint = true;
        AddOrGetQuery(true);
        return *this;
    }

protected:

    ComponentQueryInstruction& AddOrGetQuery(bool _force_add = false)
    {
        if (queries.size() == 0 || _force_add)
        {
            queries.push_back(ComponentQueryInstruction());
        }
        else
        {
            return queries.back();
        }
    }

    ComponentId                            query_owner_component;
    std::optional<WorldPosition>           query_position;
    std::vector<ComponentId>               query_component_ids;
    ComponentMask                          component_mask = 0;
    std::vector<ComponentQueryInstruction> queries;

};

JaniNamespaceBegin(Message)

struct UserConnectionRequest
{
    char     ip[16];
    uint32_t port;
    char     layer_name[128];
    uint64_t user_unique_id       = 0;
    uint32_t access_token         = 0;
    uint32_t authentication_token = 0;
};

struct WorkerConnectionRequest
{
    char     ip[16];
    uint32_t port;
    char     layer_name[128];
    uint32_t access_token          = 0;
    uint32_t worker_authentication = 0;
};

struct WorkerSpawnRequest
{
    char     runtime_ip[16];
    uint32_t runtime_listen_port = 0;
    char     layer_name[128];
};

JaniNamespaceEnd(Message)

JaniNamespaceEnd(Jani)

#undef max
#undef min