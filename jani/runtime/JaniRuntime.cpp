////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntime.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntime.h"
#include "JaniLayerCollection.h"
#include "JaniWorkerSpawnerCollection.h"
#include "JaniBridge.h"
#include "JaniDatabase.h"

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

    criar o json dos layers;
    criar a classe que le esse json;
    adicionar o interest nesse json de layers ou fazer em um separado ? ;
    criar as funcoes de query e broadcast na bridge class;
}

//
//
//

Jani::WorkerRequestResult Jani::Runtime::OnWorkerLogMessage(
    WorkerId       _worker_id,
    WorkerLogLevel _log_level,
    std::string    _log_title,
    std::string    _log_message)
{
    std::cout << "[" << magic_enum::enum_name(_log_level) << "] " << _log_title << ": " << _log_message << std::endl;

    return WorkerRequestResult(true);
}

Jani::WorkerRequestResult Jani::Runtime::OnWorkerReserveEntityIdRange(
    WorkerId _worker_id,
    uint32_t _total_ids)
{
    // Try to reserve the ids from our database
    auto reserve_entity_id_result = m_database.ReserveEntityIdRange(_total_ids);
    if (!reserve_entity_id_result)
    {
        return WorkerRequestResult(false);
    }

    return WorkerRequestResult(true); // Setup the id range result from reserve_entity_id_result.value() to reserve_entity_id_result.value() + _total_ids
}

Jani::WorkerRequestResult Jani::Runtime::OnWorkerAddEntity(
    WorkerId             _worker_id,
    EntityId             _entity_id,
    const EntityPayload& _entity_payload)
{
    // Make sure there is not active entity with the given id
    if (m_active_entities.find(_entity_id) != m_active_entities.end())
    {
        return WorkerRequestResult(false);
    }

    // Create the new entity on the database
    if (!m_database.AddEntity(_entity_id, _entity_payload))
    {
        return WorkerRequestResult(false);
    }

    // Create the new entity
    // ...

    // On the entity payload, for each component, check what bridges have interest on it and
    // proceed with any necessary broadcast
    // ...

    return WorkerRequestResult(true);
}

Jani::WorkerRequestResult Jani::Runtime::OnWorkerRemoveEntity(
    WorkerId _worker_id,
    EntityId _entity_id)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return WorkerRequestResult(false);
    }

    auto& entity = entity_iter->second;

    // For each component that this entity has, setup the removal call
    // (remover e fazer a verificacao de quais bridges escutam isso ou realmente chamar OnWorkerRemoveComponent()?)

    return WorkerRequestResult(true);
}

Jani::WorkerRequestResult Jani::Runtime::OnWorkerAddComponent(
    WorkerId                _worker_id,
    EntityId                _entity_id,
    ComponentId             _component_id,
    const ComponentPayload& _component_payload)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return WorkerRequestResult(false);
    }

    auto& entity = entity_iter->second;

    // Check if this entity already has the given component id
    if (entity.HasComponent(_component_id))
    {
        return WorkerRequestResult(false);
    }

    // Register this component on the database for the given entity
    if (!m_database.AddComponent(_entity_id, _component_id, _component_payload))
    {
        return WorkerRequestResult(false);
    }

    // Add the component to the entity
    // ...

    // Check if there are bridges that should be aware of this update
    for (auto& [bridge_layer, bridge] : m_bridges)
    {
        if (bridge->HasInterestOnComponent(_component_id))
        {
            // Broadcast component added
            // bridge->BroadcastComponentAdded(_worker_id, _entity_id, _component_id, _component_payload);
        }
    }

    return WorkerRequestResult(true);
}

Jani::WorkerRequestResult Jani::Runtime::OnWorkerRemoveComponent(
    WorkerId    _worker_id,
    EntityId    _entity_id,
    ComponentId _component_id)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return WorkerRequestResult(false);
    }

    auto& entity = entity_iter->second;

    // Check if this entity has the given component id
    if (!entity.HasComponent(_component_id))
    {
        return WorkerRequestResult(false);
    }

    // Unregister this component on the database for the given entity
    if (!m_database.RemoveComponent(_entity_id, _component_id))
    {
        return WorkerRequestResult(false);
    }

    // Remove the component to the entity
    // ...

    // Check if there are bridges that should be aware of this update
    for (auto& [bridge_layer, bridge] : m_bridges)
    {
        if (bridge->HasInterestOnComponent(_component_id))
        {
            // Broadcast component removed
            // bridge->BroadcastComponentRemoved(_worker_id, _entity_id, _component_id, _component_payload);
        }
    }

    return WorkerRequestResult(true);
}

Jani::WorkerRequestResult Jani::Runtime::OnWorkerComponentUpdate(
    WorkerId                     _worker_id,
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    const ComponentPayload&      _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return WorkerRequestResult(false);
    }

    auto& entity = entity_iter->second;

    // Check if this entity already has the given component id
    if (entity.HasComponent(_component_id))
    {
        return WorkerRequestResult(false);
    }

    // Update this component on the database for the given entity
    if (!m_database.ComponentUpdate(_entity_id, _component_id, _component_payload, _entity_world_position))
    {
        return WorkerRequestResult(false);
    }

    // Check if there are bridges that should be aware of this update
    for (auto& [bridge_layer, bridge] : m_bridges)
    {
        if (bridge->HasInterestOnComponent(_component_id))
        {
            // Broadcast component update
            // bridge->BroadcastComponentUpdated(_worker_id, _entity_id, _component_id, _component_payload);
        }
    }

    return WorkerRequestResult(true);
}

//
//
//

void Jani::Runtime::ReceiveIncommingUserConnections()
{

}

void Jani::Runtime::ApplyLoadBalanceRequirements()
{

}