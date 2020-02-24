////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"

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

Jani::Connection::Connection(uint32_t _instance_id, int _receive_port, int _dst_port, const char* _dst_address, std::optional<uint32_t> _ping_delay_ms)
{
    m_datagram_socket = std::make_unique<DatagramSocket>(_receive_port, _dst_port, _dst_address, false, true);

    m_initial_timestamp = std::chrono::steady_clock::now();

    m_ping_delay_ms = _ping_delay_ms;

    m_internal_connection = ikcp_create(_instance_id, m_datagram_socket.get());

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
                auto time_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_initial_timestamp).count();
                auto sleep_amount = ikcp_check(m_internal_connection, time_elapsed);
                auto sleep_for = sleep_amount - time_elapsed;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleep_for));

                ikcp_update(m_internal_connection, std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_initial_timestamp).count());

                if (m_datagram_socket->CanReceive())
                {
                    char buffer[MaximumDatagramSize];
                    long total = m_datagram_socket->Receive(buffer, MaximumDatagramSize);
                    if (total > 0)
                    {
                        std::lock_guard l(m_safety_mtx);

                        ikcp_input(m_internal_connection, buffer, total);
                    }
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

bool Jani::Connection::IsAlive() const
{
    return true;
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

//
//
//

