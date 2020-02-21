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
#include <magic_enum.hpp>
#include <ctti/type_id.hpp>
#include <ctti/static_value.hpp>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <nlohmann/json.hpp>

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

/////////////
// DEFINES //
/////////////

#define JaniNamespaceBegin(name)                  namespace name {
#define JaniNamespaceEnd(name)                    }

JaniNamespaceBegin(Jani)

enum class InstanceId
{
    Worker = 0
};

static const uint32_t MaximumPacketSize = 4096;

// 

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


public:
    DatagramSocket(int _receive_port, int _dst_port, const char* _dst_address, bool _broadcast, bool _reuse_socket);
    ~DatagramSocket();

    bool CanReceive();
    long Receive(char* msg, int msgsize);
    char* ReceivedFrom();
    long Send(const char* msg, int msgsize);
    long SendTo(const char* msg, int msgsize, const char* name);
    int GetAddress(const char* name, char* addr);
    const char* GetAddress(const char* name);

};

//
//
//

#ifdef _WIN32
typedef int socklen_t;
#endif



//
//
//

class Connection
{
public:

    Connection(uint32_t _instance_id, int _receive_port, int _dst_port, const char* _dst_address, std::optional<uint32_t> _ping_delay_ms = std::nullopt);
    ~Connection();

    void Update();

    bool IsAlive() const;

    std::optional<size_t> Receive(char* _msg, int _buffer_size);
    std::optional<size_t> Send(const char* _msg, int _msg_size) const;

private:

    std::unique_ptr<DatagramSocket>                    m_datagram_socket;
    std::optional<uint32_t>                            m_ping_delay_ms;
    std::chrono::time_point<std::chrono::steady_clock> m_initial_timestamp;
    ikcpcb*                                            m_internal_connection = nullptr;
    std::thread                                        m_update_thread;
    bool                                               m_exit_update_thread  = false;
    mutable std::mutex                                 m_safety_mtx;
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

    BridgeInterface(int _receive_port, int _bridge_port, const char* _bridge_address, uint32_t _ping_delay_ms) :
        m_bridge_connection(magic_enum::enum_integer(InstanceId::Worker), _receive_port, _bridge_port, _bridge_address, _ping_delay_ms),
        m_temporary_data_buffer(std::array<char, MaximumPacketSize>())
    {

    }

    std::optional<std::pair<const char*, size_t>> PoolIncommingData()
    {
        auto total = m_bridge_connection.Receive(m_temporary_data_buffer.data(), m_temporary_data_buffer.size());
        if (total)
        {
            return std::make_pair(m_temporary_data_buffer.data(), total.value());
        }

        return std::nullopt;
    }

    bool PushData(const char* _data, size_t _data_size)
    {
        auto result = m_bridge_connection.Send(_data, _data_size);

        return result != std::nullopt;
    }

private:

    Connection                          m_bridge_connection;
    std::array<char, MaximumPacketSize> m_temporary_data_buffer;
};

JaniNamespaceEnd(Jani)

#undef max
#undef min