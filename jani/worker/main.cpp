////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"

int main(int _argc, char* _argv[])
{
    int from;
    int to;
    bool is_server = _argc == 4;
    if (is_server)
    {
        from = 8080;
        to   = 8081;   
    }
    else
    {
        from = 8081;
        to   = 8080;
    }

    Jani::Connection connection(5, from, to, "127.0.0.1");

    while (true)
    {
        if (is_server)
        {
            std::string send_value;
            std::cout << "Send: ";
            std::cin >> send_value;
            std::cout << std::endl;

            if (send_value == "exit")
            {
                break;
            }

            connection.Send(send_value.data(), send_value.size());
            std::cout << "Sent: " << send_value << " {" << send_value.size() << "}" << std::endl;
        }
        else
        {
            char buffer[1024];
            auto total = connection.Receive(buffer, 1024);
            if (total)
            {
                std::cout << "Received: " << std::string(buffer, total.value()) << " {" << total.value() << "}" << std::endl;
            }
        }
    }

    return 0;
}