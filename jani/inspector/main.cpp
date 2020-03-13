////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"
#include "InspectorManager.h"
#include "JaniLayerCollection.h"

int main(int _argc, char* _argv[])
{
    Jani::InitializeStandardConsole();

    const char*   runtime_ip          = "127.0.0.1";
    uint32_t      runtime_listen_port = 14051;

    Jani::MessageLog().Info("Inspector -> Connected with Runtime at runtime_ip {} , runtime_listen_port {}, layer_id {}", runtime_ip, runtime_listen_port);

    std::unique_ptr<Jani::LayerCollection> layer_collection = std::make_unique<Jani::LayerCollection>();

    if (!layer_collection->Initialize("layers_config.json"))
    {
        Jani::MessageLog().Critical("Inspector -> LayerCollection failed to initialize");

        return false;
    }

    {
        Jani::Inspector::InspectorManager inspector_manager(*layer_collection);

        if (inspector_manager.Initialize(runtime_ip, runtime_listen_port))
        {
            while (!inspector_manager.ShouldDisconnect())
            {
                std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();

                inspector_manager.Update(3);

                auto process_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() > 5)
                {
                    Jani::MessageLog().Warning("Inspector -> Update frame is taking too long to process process_time {}ms", process_time);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(3));
            }
        }
    }

    Jani::MessageLog().Warning("Inspector -> Disconnected from the server");

    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    return 0;
}