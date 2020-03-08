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

std::string pretty_bytes(uint64_t _bytes)
{
    char out_buffer[64];
    const char* suffixes[7];
    suffixes[0] = "B";
    suffixes[1] = "KB";
    suffixes[2] = "MB";
    suffixes[3] = "GB";
    suffixes[4] = "TB";
    suffixes[5] = "PB";
    suffixes[6] = "EB";
    uint64_t s = 0; // which suffix to use
    double count = _bytes;
    while (count >= 1024 && s < 7)
    {
        s++;
        count /= 1024;
    }
    if (count - floor(count) == 0.0)
        sprintf(out_buffer, "%d %s", (int)count, suffixes[s]);
    else
        sprintf(out_buffer, "%.1f %s", count, suffixes[s]);

    return std::string(out_buffer);
}

void* operator new(size_t _size)
{
    s_total_allocations_per_second++;
    s_total_allocated_per_second += _size;

    return malloc(_size);
}

int main(int _argc, char* _argv[])
{
    std::unique_ptr<Jani::Database> database = std::make_unique<Jani::Database>();
    std::unique_ptr<Jani::Runtime>  runtime;

    {
        std::unique_ptr<Jani::LayerCollection>         layer_collection          = std::make_unique<Jani::LayerCollection>();
        std::unique_ptr<Jani::WorkerSpawnerCollection> worker_spawner_collection = std::make_unique<Jani::WorkerSpawnerCollection>();
        std::unique_ptr<Jani::DeploymentConfig>        deployment_config         = std::make_unique<Jani::DeploymentConfig>();

        if (!layer_collection->Initialize("layers_config.json"))
        {
            std::cout << "LayerCollection -> Failed to initialize" << std::endl;

            return false;
        }

        if (!worker_spawner_collection->Initialize("worker_spawners_config.json"))
        {
            std::cout << "WorkerSpawnerCollection -> Failed to initialize" << std::endl;

            return false;
        }

        if (!deployment_config->Initialize("deployment_config.json"))
        {
            std::cout << "DeploymentConfig -> Failed to initialize" << std::endl;

            return false;
        }

        runtime = std::make_unique<Jani::Runtime>(
            *database, 
            *deployment_config, 
            *layer_collection,
            *worker_spawner_collection);

        if (!runtime->Initialize())
        {
            std::cout << "Runtime -> Problem initializing runtime, verify if your config file is valid" << std::endl;

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
                std::cout << "Runtime -> Update frame is taking too long to process process_time{" << process_time << "}" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            elapsed_time_since_last_memory_feedback += std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();
            if (elapsed_time_since_last_memory_feedback > 1000)
            {
                std::cout << "Total allocations: {" << s_total_allocations_per_second << "}, a total of: " << pretty_bytes(s_total_allocated_per_second) << std::endl;
                s_total_allocations_per_second = 0;
                s_total_allocated_per_second = 0;
                elapsed_time_since_last_memory_feedback = 0;
            }
        }
    }

    return 0;
}