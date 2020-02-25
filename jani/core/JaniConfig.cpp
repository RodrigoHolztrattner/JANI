////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"

#if 0

Jani::DatagramSocket::DatagramSocket(int _receive_port, int _dst_port, const char* _dst_address, bool _broadcast, bool _reuse_socket)
{
    receive_port = _receive_port;
    dst_port     = _dst_port;

#ifdef _WIN32
    retval = WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    sockaddr_in addr;
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    //set up bind address
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(_receive_port);

    //set up address to use for sending
    memset(&outaddr, 0, sizeof(outaddr));
    outaddr.sin_family = AF_INET;
    outaddr.sin_addr.s_addr = _dst_address == nullptr ? INADDR_ANY : inet_addr(_dst_address);
    outaddr.sin_port = htons(_dst_port);

    if(_receive_port == 0)
    {
        return;
    }

#ifdef _WIN32
    bool bOptVal = 1;
    int bOptLen = sizeof(bool);
#else
    int OptVal = 1;
#endif

    if (_broadcast)
#ifdef _WIN32
        retval = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, bOptLen);
#else
        retval = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &OptVal, sizeof(OptVal));
#endif

    if (_reuse_socket)
#ifdef _WIN32
        retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&bOptVal, bOptLen);
#else
        retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &OptVal, sizeof(OptVal));
#endif

#ifdef _WIN32
    struct timeval read_timeout;
    read_timeout.tv_sec = 1;
    read_timeout.tv_usec = 1;
    retval = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char*)&read_timeout, sizeof(read_timeout));
#else
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 1;
    retval = setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof(read_timeout));
#endif

    retval = bind(sock, (struct sockaddr*) & addr, sizeof(addr));
}

Jani::DatagramSocket::~DatagramSocket()
{
#if _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif
}

int Jani::DatagramSocket::GetAddress(const char* name, char* addr)
{
    struct hostent* hp;
    if ((hp = gethostbyname(name)) == NULL) return (0);
    strcpy(addr, inet_ntoa(*(struct in_addr*)(hp->h_addr)));
    return (1);
}

const char* Jani::DatagramSocket::GetAddress(const char* name)
{
    struct hostent* hp;
    if ((hp = gethostbyname(name)) == NULL) return (0);
    strcpy(ip, inet_ntoa(*(struct in_addr*)(hp->h_addr)));
    return ip;
}

uint32_t Jani::DatagramSocket::GetReceivePort() const
{
    return receive_port;
}

uint32_t Jani::DatagramSocket::GetDstPort() const
{
    return dst_port;
}

bool Jani::DatagramSocket::CanReceive()
{
    fd_set          sready;
    struct timeval  nowait;
    FD_ZERO(&sready);
    FD_SET(sock, &sready);
    memset((char*)&nowait, 0, sizeof(nowait));

    bool res = select(sock, &sready, NULL, NULL, &nowait);
    if (FD_ISSET(sock, &sready))
        res = true;
    else
        res = false;

    return res;
}

long Jani::DatagramSocket::Receive(char* msg, int msgsize)
{
    struct sockaddr_in sender;
    socklen_t sendersize = sizeof(sender);
    int retval = recvfrom(sock, msg, msgsize, 0, (struct sockaddr*) & sender, &sendersize);
    strcpy(received, inet_ntoa(sender.sin_addr));
    return retval;
}

char* Jani::DatagramSocket::ReceivedFrom()
{
    return received;
}

long Jani::DatagramSocket::Send(const char* msg, int msgsize) const
{
    return sendto(sock, msg, msgsize, 0, (struct sockaddr*) & outaddr, sizeof(outaddr));
}

long Jani::DatagramSocket::SendTo(const char* msg, int msgsize, const char* addr)
{
    outaddr.sin_addr.s_addr = inet_addr(addr);
    return sendto(sock, msg, msgsize, 0, (struct sockaddr*) & outaddr, sizeof(outaddr));
}

//
//
//

Jani::Connection::Connection(uint32_t _instance_id, int _receive_port, int _dst_port, const char* _dst_address, std::optional<uint32_t> _heartbeat_ms)
{
    m_datagram_socket     = std::make_unique<DatagramSocket>(_receive_port, _dst_port, _dst_address, false, true);
    m_initial_timestamp   = std::chrono::steady_clock::now();
    m_heartbeat_ms        = _heartbeat_ms;
    m_internal_connection = ikcp_create(_instance_id, m_datagram_socket.get());

    // Update the timeout to be at least 2* the heartbeat
    if (m_heartbeat_ms && m_heartbeat_ms.value() > m_timeout_amount_ms / 2)
    {
        m_timeout_amount_ms = m_heartbeat_ms.value() * 2;
    }

    ikcp_nodelay(m_internal_connection, 1, 10, 2, 1);

    ikcp_setoutput(
        m_internal_connection,
        [](const char* buf, int len, ikcpcb* kcp, void* user) -> int
        {
            Jani::DatagramSocket* socket = (Jani::DatagramSocket*)user;

            long total_sent = socket->Send(buf, len);
            if (total_sent == -1)
            {
                // auto error_n = WSAGetLastError();
                // error!
            }
            return total_sent;
        });

    m_update_thread = std::thread(
        [&]()
        {
            while (!m_exit_update_thread)
            {
                auto time_now                  = std::chrono::steady_clock::now();
                auto time_elapsed              = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_initial_timestamp).count();
                auto target_update_time        = ikcp_check(m_internal_connection, time_elapsed);
                auto time_remaining_for_update = target_update_time - time_elapsed;
                std::this_thread::sleep_for(std::chrono::milliseconds(time_remaining_for_update));

                m_last_update_timestamp = time_now;

                ikcp_update(m_internal_connection, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_initial_timestamp).count());

                if (m_datagram_socket->CanReceive())
                {
                    char buffer[MaximumDatagramSize];
                    long total = m_datagram_socket->Receive(buffer, MaximumDatagramSize);
                    if (total > 0)
                    {
                        std::lock_guard l(m_safety_mtx);
                        std::cout << "Received something" << std::endl;
                        m_last_receive_timestamp = time_now;
                        m_is_waiting_for_ping    = false;
                        ikcp_input(m_internal_connection, buffer, total);
                    }
                }

                if (m_heartbeat_ms 
                    && !m_is_waiting_for_ping 
                    && std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_receive_timestamp).count() > m_heartbeat_ms.value())
                {
                    // Send a ping message
                    uint32_t    ping_message_size = 0;
                    const char* ping_message      = GetPingDatagram(ping_message_size);
                    std::cout << "Send ping" << std::endl;

                    m_is_waiting_for_ping = Send(ping_message, ping_message_size) != std::nullopt;
                }
            }
        });
}

Jani::Connection::~Connection()
{
    m_exit_update_thread = true;
    if (m_update_thread.joinable())
    {
        m_update_thread.join();
    }
}

void Jani::Connection::Update()
{
    std::chrono::time_point<std::chrono::steady_clock> m_last_update_timestamp;
    std::chrono::time_point<std::chrono::steady_clock> m_last_receive_timestamp;

    return; // Using the update method for connection updates needs to be tested first!

    auto time_now                  = std::chrono::steady_clock::now();
    auto time_elapsed              = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_initial_timestamp).count();
    auto target_update_time        = ikcp_check(m_internal_connection, time_elapsed);
    auto time_remaining_for_update = target_update_time - time_elapsed;
    if (time_remaining_for_update <= 0)
    {
        m_last_update_timestamp = time_now;

        ikcp_update(m_internal_connection, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_initial_timestamp).count());

        if (m_datagram_socket->CanReceive())
        {
            char buffer[MaximumDatagramSize];
            long total = m_datagram_socket->Receive(buffer, MaximumDatagramSize);
            if (total > 0)
            {
                std::lock_guard l(m_safety_mtx);

                m_last_receive_timestamp = time_now;
                m_is_waiting_for_ping    = false;
                ikcp_input(m_internal_connection, buffer, total);
            }
        }

        if (m_heartbeat_ms 
            && !m_is_waiting_for_ping 
            && std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_receive_timestamp).count() > m_heartbeat_ms.value())
        {
            // Send a ping message
            uint32_t    ping_message_size = 0;
            const char* ping_message      = GetPingDatagram(ping_message_size);

            m_is_waiting_for_ping = Send(ping_message, ping_message_size) != std::nullopt;
        }
    }
}

void Jani::Connection::SetTimeoutRequired(uint32_t _timeout_ms)
{
    m_timeout_amount_ms = _timeout_ms;

    if (m_heartbeat_ms && m_heartbeat_ms.value() > m_timeout_amount_ms / 2)
    {
        m_timeout_amount_ms = m_heartbeat_ms.value() * 2;
    }
}

bool Jani::Connection::DidTimeout() const
{
    std::lock_guard l(m_safety_mtx);

    /*
    * If for some reason no update was called, that could have caused a fake timeout, that's why
    * the time difference from the previous update should be taken in consideration
    */
    auto time_now                 = std::chrono::steady_clock::now();
    auto time_from_last_receive   = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_receive_timestamp).count();
    auto time_from_last_update    = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
    auto time_elapsed_for_timeout = time_from_last_receive - time_from_last_update;

    if (m_is_waiting_for_ping && time_elapsed_for_timeout > m_heartbeat_ms)
    {
        return true;
    }

    return false;
}

uint32_t Jani::Connection::GetReceiverPort() const
{
    return m_datagram_socket->GetReceivePort();
}

uint32_t Jani::Connection::GetDestinationPort() const
{
    return m_datagram_socket->GetDstPort();
}

Jani::ConnectionListener::ConnectionListener(int _listen_port)
{
    m_datagram_socket = std::make_unique<DatagramSocket>(_listen_port, 0, nullptr, false, true);
}

Jani::ConnectionListener::~ConnectionListener()
{
}

#endif

//
//
//

Jani::Connection::Connection(int _local_port, int _dst_port, const char* _dst_address)
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

    m_single_kcp_instance = ikcp_create(0, &m_socket);
    if (!m_single_kcp_instance)
    {
        return;
    }

    m_is_valid = true;
}

Jani::Connection::Connection(int _local_port, std::optional<AuthenticationCallback> _authentication_callback)
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

void Jani::Connection::Update()
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

void Jani::Connection::DidTimeout(const TimeoutCallback& _timeout_callback) const
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

bool Jani::Connection::Send(const void* _msg, int _msg_size, ClientHash _client_hash) const
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

void Jani::Connection::Receive(const ReceiveCallback& _receive_callback) const
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

bool Jani::Connection::CanReceiveDatagram() const
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

void Jani::Connection::TryReceiveDatagrams()
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
                new_client.hash         = client_hash;
                new_client.kcp_instance = ikcp_create(0, &m_socket);
                new_client.address      = inet_ntoa(sender.sin_addr);
                new_client.port         = sender.sin_port;
                if (!new_client.kcp_instance)
                {
                    continue;
                }

                client_iter = m_server_clients.insert({ client_hash, std::move(new_client) }).first;
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

bool Jani::Connection::SetupListenSocket()
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

Jani::Connection::ClientHash Jani::Connection::HashClientAddr(const struct sockaddr_in& _client_addr) const
{
    return static_cast<ClientHash>(_client_addr.sin_addr.S_un.S_addr ^ _client_addr.sin_port | _client_addr.sin_port >> 16);
}

bool Jani::Connection::IsPingDatagram(const char* _message, uint32_t _message_size) const
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

const char* Jani::Connection::GetPingDatagram(uint32_t& _size) const
{
    static const char ping_datagram[5] = { 4, 28, 36, 19, 111 };
    _size = sizeof(ping_datagram);
    return ping_datagram;
}