////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntime.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntime.h"
#include "JaniLayerCollection.h"
#include "JaniWorkerSpawnerCollection.h"
#include "JaniBridge.h"

Jani::Runtime::Runtime(Database& _database) :
    m_database(_database)
{
}

Jani::Runtime::~Runtime()
{
}

bool Jani::Runtime::Initialize(
    std::unique_ptr<LayerCollection>         layer_collection,
    std::unique_ptr<WorkerSpawnerCollection> worker_spawner_collection)
{
    assert(layer_collection);
    assert(worker_spawner_collection);

    if (!layer_collection->IsValid()
        || !worker_spawner_collection->IsValid())
    {
        return false;
    }

    m_layer_collection          = std::move(layer_collection);
    m_worker_spawner_collection = std::move(worker_spawner_collection);

    return true;
}

void Jani::Runtime::Update()
{
    ReceiveIncommingUserConnections();

    ApplyLoadBalanceRequirements();
    /*
    for (auto& bridge : m_bridges)
    {
        bridge->Update();
    }
    */
}

void Jani::Runtime::ReceiveIncommingUserConnections()
{

}

void Jani::Runtime::ApplyLoadBalanceRequirements()
{

}