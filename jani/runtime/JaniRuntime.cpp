////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntime.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntime.h"
#include "JaniLayerCollection.h"
#include "JaniWorkerSpawnerCollection.h"
#include "JaniBridge.h"
#include "JaniDatabase.h"
#include "JaniWorkerInstance.h"

const char* LocalIp = "127.0.0.1";

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

    m_client_connections = std::make_unique<Connection<Message::UserConnectionRequest>>(
        8080, 
        [&](auto _client_hash, const Message::UserConnectionRequest& _connection_request)
        {
            auto layer_id = Hasher(_connection_request.layer_name);

            auto bridge_iter = m_bridges.find(layer_id);
            if (bridge_iter == m_bridges.end())
            {
                auto new_bridge = std::make_unique<Bridge>(*this, layer_id);
                m_bridges.insert({ layer_id , std::move(new_bridge) });
                bridge_iter = m_bridges.find(layer_id);
            }

            auto& bridge = bridge_iter->second;

            // Determine if this bridge can accept another layer (if the balance strategy allows it
            // TODO: ...

            static uint32_t local_port = 9090; // TODO: Use a decent port-select function

            auto user_worker_instance = std::make_unique<WorkerInstance>(
                _connection_request.ip, 
                _connection_request.port, 
                local_port++, 
                layer_id,
                true);

            if (!bridge->RegisterNewWorkerInstance(std::move(user_worker_instance)))
            {
                return false;
            }

            return true;
        });

    m_worker_connections = std::make_unique<Connection<Message::WorkerConnectionRequest>>(
        13051,
        [&](auto _client_hash, const Message::WorkerConnectionRequest& _connection_request)
        {
            auto layer_id = Hasher(_connection_request.layer_name);

            auto bridge_iter = m_bridges.find(layer_id);
            if (bridge_iter == m_bridges.end())
            {
                auto new_bridge = std::make_unique<Bridge>(*this, layer_id);
                m_bridges.insert({ layer_id , std::move(new_bridge) });
                bridge_iter = m_bridges.find(layer_id);
            }

            auto& bridge = bridge_iter->second;

            // Determine if this bridge can accept another layer (if the balance strategy allows it
            // TODO: ...

            static uint32_t local_port = 11090; // TODO: Use a decent port-select function

            auto worker_instance = std::make_unique<WorkerInstance>(
                _connection_request.ip,
                _connection_request.port,
                local_port++, 
                layer_id,
                false);

            if (!bridge->RegisterNewWorkerInstance(std::move(worker_instance)))
            {
                // Big problem!
                // ...
            }

            return true;
        });

    auto& worker_spawners = m_worker_spawner_collection->GetWorkerSpawnersInfos();
    for (auto& worker_spawner_info : worker_spawners)
    {
        static uint32_t spawner_port = 13000;
        auto spawner_connection = std::make_unique<Connection<>>(
            spawner_port++, 
            worker_spawner_info.port, 
            worker_spawner_info.ip.c_str());

        m_worker_spawner_connections.push_back(std::move(spawner_connection));
    }

    return true;
}

void Jani::Runtime::Update()
{
    m_client_connections->Update();
    m_worker_connections->Update();
    
    for (auto& worker_spawner_connection : m_worker_spawner_connections)
    {
        worker_spawner_connection->Update();
    }

    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_load_balance_previous_update_time).count() > 1000)
    {
        ApplyLoadBalanceUpdate();
        m_load_balance_previous_update_time = std::chrono::steady_clock::now();
    }

    for (auto& [layer_hash, bridge] : m_bridges)
    {
        bridge->Update();
    }

    for (auto& spawner_connection : m_worker_spawner_connections)
    {
        spawner_connection->Update();
    }
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
    auto& entity = m_active_entities.insert({ _entity_id, Entity(_entity_id) }).first->second;

    // Add each component contained on the payload
    for (auto& component_payload : _entity_payload.component_payloads)
    {
        entity.AddComponent(component_payload.component_id);
    }

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

    if (!m_database.RemoveEntity(_entity_id))
    {
        return WorkerRequestResult(false);
    }

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
    entity.AddComponent(_component_id);

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

    // Remove the component from the entity
    entity.RemoveComponent(_component_id);

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

    return WorkerRequestResult(true);
}

Jani::WorkerRequestResult Jani::Runtime::OnWorkerComponentQuery(
    WorkerId              _worker_id,
    EntityId              _entity_id,
    const ComponentQuery& _component_query)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return WorkerRequestResult(false);
    }

    // Dumb query search
    std::vector<EntityId> selected_entities;
    for (auto& query : _component_query.queries)
    {
        for (auto& [entity_id, entity]: m_active_entities)
        {
            if (entity.HasComponents(query.component_constraints_mask))
            {
                if (!query.area_constraint && !query.radius_constraint)
                {
                    selected_entities.push_back(entity_id);
                }
                else if (query.area_constraint && _component_query.query_position)
                {
                    WorldRect search_rect = { _component_query.query_position->x, _component_query.query_position->y, query.area_constraint->width, query.area_constraint->height };
                    if (search_rect.ManhattanDistanceFromPosition(entity.GetRepresentativeWorldPosition()) == 0)
                    {
                        selected_entities.push_back(entity_id);
                    }
                }
                else if (query.radius_constraint && _component_query.query_position) // query.area_constraint->ManhattanDistanceFromPosition(entity.GetRepresentativeWorldPosition()) == 0)
                {
                    int32_t diffY    = entity.GetRepresentativeWorldPosition().y - _component_query.query_position->y;
                    int32_t diffX    = entity.GetRepresentativeWorldPosition().x - _component_query.query_position->x;
                    int32_t distance = std::sqrt((diffY * diffY) + (diffX * diffX));
                    if (distance < query.radius_constraint.value())
                    {
                        selected_entities.push_back(entity_id);
                    }
                }
            }
        }
    }
    
    // For each selected entity, get the requested components and prepare the return data
    // ...

    return WorkerRequestResult(false);
}

void Jani::Runtime::ApplyLoadBalanceUpdate()
{
    auto& worker_spawners   = m_worker_spawner_collection->GetWorkerSpawnersInfos();
    auto& registered_layers = m_layer_collection->GetLayers();

    static bool should_run = true;

    for (auto& [layer_id, layer_info] : registered_layers)
    {
        if (!should_run)
        {
            break;
        }

        if (!layer_info.is_user_layer)
        {
            // Make sure we have one bridge that owns managing this layer
            auto bridge_iter = m_bridges.find(layer_id);
            if (bridge_iter == m_bridges.end())
            {
                auto new_bridge = std::make_unique<Bridge>(*this, layer_id);
                m_bridges.insert({ layer_id , std::move(new_bridge) });
                bridge_iter = m_bridges.find(layer_id);
            }

            auto& bridge = bridge_iter->second;

            // We must have at least one worker on this bridge
            if (bridge->GetTotalWorkerCount() == 0 && m_worker_spawner_connections.size() > 0)
            {
                Message::WorkerSpawnRequest spawn_request;

                std::strcpy(spawn_request.runtime_ip, LocalIp);
                std::strcpy(spawn_request.layer_name, layer_info.name.data());
                spawn_request.runtime_listen_port = 13051;

                if (!m_worker_spawner_connections.back()->Send(&spawn_request, sizeof(Message::WorkerSpawnRequest)))
                {
                    // Error!
                }
            }
        }
    }

    should_run = false;
    /*
        => O que essa funcao deve fazer:

        - Ela deve rodar tipo uma vez a cada segundo (nao muito frequente)

        - Deve verificar se existe pelo menos 1 layer de cada tipo ativado, caso nao tenha ele deve
        requisitar a criacao dos workers necessarios
        - Deve verificar dentre os layers/bridges existentes se alguma esta sobrecarregada, requisitando
        a criacao de um novo worker para o layer em questao
    */

    // m_worker_spawner_collection
}