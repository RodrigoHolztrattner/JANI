////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniConfig.h"
#include "JaniDatabase.h"
#include "JaniRuntime.h"
#include "JaniLayerCollection.h"
#include "JaniWorkerSpawnerCollection.h"
#include "JaniDeploymentConfig.h"

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

        while (true)
        {
            runtime->Update();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

    return 0;
}