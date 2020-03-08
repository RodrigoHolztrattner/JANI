////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"
#include "InspectorManager.h"

int main(int _argc, char* _argv[])
{
    const char*   runtime_ip          = "127.0.0.1";
    uint32_t      runtime_listen_port = 14051;

    std::cout << "Inspector -> Connected with Runtime at runtime_ip{" << runtime_ip << "}, runtime_listen_port{" << runtime_listen_port << "}" << std::endl;

    {
        Jani::Inspector::InspectorManager inspector_manager;

        if (inspector_manager.Initialize(runtime_ip, runtime_listen_port))
        {
            while (!inspector_manager.ShouldDisconnect())
            {
                std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();

                inspector_manager.Update(3);

                auto process_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() > 5)
                {
                    std::cout << "Inspector -> Update frame is taking too long to process process_time{" << process_time << "}" << std::endl;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }
        }
    }

    std::cout << "Inspector -> Disconnected from the server!" << std::endl;

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    return 0;
}