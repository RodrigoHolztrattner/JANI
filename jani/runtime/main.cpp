////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"
#include "JaniDatabase.h"
#include "JaniRuntime.h"
#include "JaniLayerCollection.h"
#include "JaniWorkerSpawnerCollection.h"
#include "JaniDeploymentConfig.h"

inline static uint64_t s_total_allocations_per_second = 0;
inline static uint64_t s_total_allocated_per_second   = 0;

void* operator new(size_t _size)
{
    s_total_allocations_per_second++;
    s_total_allocated_per_second += _size;

    return malloc(_size);
}

int main(int _argc, char* _argv[])
{
    Jani::InitializeStandardConsole();

    std::unique_ptr<Jani::Database> database = std::make_unique<Jani::Database>();
    std::unique_ptr<Jani::Runtime>  runtime;

    {
        std::unique_ptr<Jani::LayerCollection>         layer_collection          = std::make_unique<Jani::LayerCollection>();
        std::unique_ptr<Jani::WorkerSpawnerCollection> worker_spawner_collection = std::make_unique<Jani::WorkerSpawnerCollection>();
        std::unique_ptr<Jani::DeploymentConfig>        deployment_config         = std::make_unique<Jani::DeploymentConfig>();

        if (!layer_collection->Initialize("layers_config.json"))
        {
            Jani::MessageLog().Critical("Runtime -> LayerCollection failed to initialize");

            return false;
        }

        if (!worker_spawner_collection->Initialize("worker_spawners_config.json"))
        {
            Jani::MessageLog().Critical("Runtime -> WorkerSpawnerCollection failed to initialize");

            return false;
        }

        if (!deployment_config->Initialize("deployment_config.json"))
        {
            Jani::MessageLog().Critical("Runtime -> DeploymentConfig failed to initialize");

            return false;
        }

        runtime = std::make_unique<Jani::Runtime>(
            *database, 
            *deployment_config, 
            *layer_collection,
            *worker_spawner_collection);

        if (!runtime->Initialize())
        {
            Jani::MessageLog().Critical("Runtime -> Problem initializing runtime, verify if your config file is valid");

            return false;
        }

        uint64_t elapsed_time_since_last_memory_feedback = 0;

        while (true)
        {
            std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();

            runtime->Update();

            auto process_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() > 3)
            {
                Jani::MessageLog().Warning("Runtime -> Update frame is taking too long to process process_time({})", process_time);
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            elapsed_time_since_last_memory_feedback += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
            if (elapsed_time_since_last_memory_feedback > 1000)
            {
                Jani::MessageLog().Info("Total allocations: ({}), a total of: ({})", s_total_allocations_per_second, Jani::pretty_bytes(s_total_allocated_per_second));

                s_total_allocations_per_second          = 0;
                s_total_allocated_per_second            = 0;
                elapsed_time_since_last_memory_feedback = 0;
            }
        }
    }

    return 0;
}