////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntime.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntime.h"
#include "JaniLayerCollection.h"
#include "JaniWorkerSpawnerCollection.h"
#include "JaniBridge.h"
#include "JaniDatabase.h"
#include "JaniWorkerInstance.h"
#include "JaniWorkerSpawnerInstance.h"

const char* s_local_ip                  = "127.0.0.1";
uint32_t    s_client_worker_listen_port = 8080;
uint32_t    s_worker_listen_port        = 13051;

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

    m_client_connections = std::make_unique<Connection<>>(s_client_worker_listen_port);
    m_worker_connections = std::make_unique<Connection<>>(s_worker_listen_port);
    m_request_manager    = std::make_unique<RequestManager>();

    auto& worker_spawners = m_worker_spawner_collection->GetWorkerSpawnersInfos();
    for (auto& worker_spawner_info : worker_spawners)
    {
        static uint32_t spawner_port = 13000;

        auto spawner_connection = std::make_unique<WorkerSpawnerInstance>();

        if (!spawner_connection->Initialize(
            spawner_port++,
            worker_spawner_info.port,
            worker_spawner_info.ip.c_str(),
            s_worker_listen_port,
            s_local_ip))
        {
            return false;
        }

        m_worker_spawner_instances.push_back(std::move(spawner_connection));
    }

    return true;
}

void Jani::Runtime::Update()
{
    m_client_connections->Update();
    m_worker_connections->Update();

    auto ProcessWorkerRequest = [&](
        auto                         _client_hash, 
        bool                         _is_user, 
        const Request&               _request, 
        cereal::BinaryInputArchive&  _request_payload, 
        cereal::BinaryOutputArchive& _response_payload)
    {
        // If this is an authentication request, process it, else delegate the request to the worker itself
        // [[unlikely]]
        if (_request.type == RequestType::ClientWorkerAuthentication)
        {
            Message::ClientWorkerAuthenticationRequest authentication_request;
            {
                _request_payload(authentication_request);
            }

            // Authenticate this worker
            // ...

            bool worker_allocation_result = TryAllocateNewWorker(
                Hasher(authentication_request.layer_name),
                _client_hash,
                true);

            std::cout << "Runtime -> New client worker connected" << std::endl;

            Message::ClientWorkerAuthenticationResponse authentication_response = { worker_allocation_result };
            {
                _response_payload(authentication_response);
            }
        }
        // [[unlikely]]
        else if (_request.type == RequestType::WorkerAuthentication)
        {
            Message::WorkerAuthenticationRequest authentication_request;
            {
                _request_payload(authentication_request);
            }

            // Authenticate this worker
            // ...
            
            bool worker_allocation_result = TryAllocateNewWorker(
                authentication_request.layer_hash,
                _client_hash,
                false);

            std::cout << "Runtime -> New worker connected" << std::endl;

            Message::WorkerAuthenticationResponse authentication_response = { worker_allocation_result };
            {
                _response_payload(authentication_response);
            }
        }
        else
        {
            auto worker_instance_iter = m_worker_instance_mapping.find(_client_hash);
            // [[likely]]
            if (worker_instance_iter != m_worker_instance_mapping.end())
            {
                worker_instance_iter->second->ProcessRequest(
                    _request,
                    _request_payload,
                    _response_payload);
            }
        }
    };

    m_request_manager->CheckRequests(
        *m_client_connections, 
        [&](auto _client_hash, const Request& _request, cereal::BinaryInputArchive& _request_payload, cereal::BinaryOutputArchive& _response_payload)
        {
            ProcessWorkerRequest(
                _client_hash,
                true,
                _request,
                _request_payload,
                _response_payload);
        });

    m_request_manager->CheckRequests(
        *m_worker_connections,
        [&](auto _client_hash, const Request& _request, cereal::BinaryInputArchive& _request_payload, cereal::BinaryOutputArchive& _response_payload)
        {
            ProcessWorkerRequest(
                _client_hash,
                false,
                _request,
                _request_payload,
                _response_payload);
        });

    auto ProcessWorkerDisconnection = [&](auto _client_hash)
    {
        auto worker_instance_iter = m_worker_instance_mapping.find(_client_hash);
        if (worker_instance_iter == m_worker_instance_mapping.end())
        {
            return;
        }

        auto* worker_instance  = worker_instance_iter->second;
        auto worker_layer_hash = worker_instance->GetLayerHash();

        m_worker_instance_mapping.erase(worker_instance_iter);
        
        auto bridge_iter = m_bridges.find(worker_layer_hash);
        if (bridge_iter == m_bridges.end())
        {
            return;
        }

        auto* bridge = bridge_iter->second.get();

        bool disconnect_result = bridge->DisconnectWorker(_client_hash);
        if (!disconnect_result)
        {
            std::cout << "Runtime -> Error trying to disconnect worker, its not registered on the bridge" << std::endl;
        }
        else
        {
            std::cout << "Runtime -> Worker disconnected" << std::endl;
        }
    };

    m_client_connections->DidTimeout(
        [&](std::optional<Connection<>::ClientHash> _client_hash)
        {
            assert(_client_hash.has_value());
            ProcessWorkerDisconnection(_client_hash.value());
        });

    m_worker_connections->DidTimeout(
        [&](std::optional<Connection<>::ClientHash> _client_hash)
        {
            assert(_client_hash.has_value());
            ProcessWorkerDisconnection(_client_hash.value());
        });

    for (auto& worker_spawner_connection : m_worker_spawner_instances)
    {
        worker_spawner_connection->Update();
    }

    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_load_balance_previous_update_time).count() > 10000)
    {
        ApplyLoadBalanceUpdate();
        m_load_balance_previous_update_time = std::chrono::steady_clock::now();
    }

    for (auto& [layer_hash, bridge] : m_bridges)
    {
        bridge->Update();
    }
}

bool Jani::Runtime::TryAllocateNewWorker(
    LayerHash                _layer_hash,
    Connection<>::ClientHash _client_hash,
    bool                     _is_user)
{
    assert(m_worker_instance_mapping.find(_client_hash) == m_worker_instance_mapping.end());

    if (!m_layer_collection->HasLayer(_layer_hash))
    {
        return false;
    }

    auto layer_info = m_layer_collection->GetLayerInfo(_layer_hash);

    // Make sure the layer is an user layer if the requester is also an user, also vice-versa
    if (layer_info.is_user_layer != _is_user)
    {
        return false;
    }

    // Make sure that this layer support at least one worker
    if (layer_info.load_balance_strategy.maximum_workers && layer_info.load_balance_strategy.maximum_workers.value() == 0)
    {
        return false;
    }

    // Ok we are free to at least attempt to retrieve (or create) a bridge for this layer //
    auto bridge_iter = m_bridges.find(_layer_hash);
    if (bridge_iter == m_bridges.end())
    {
        auto new_bridge = std::make_unique<Bridge>(*this, _layer_hash);
        m_bridges.insert({ _layer_hash , std::move(new_bridge) });
        bridge_iter = m_bridges.find(_layer_hash);
    }

    auto& bridge = bridge_iter->second;

    auto worker_allocation_result = bridge->TryAllocateNewWorker(_layer_hash, _client_hash, _is_user);
    if (!worker_allocation_result)
    {
        return false;
    }

    // Add an entry into our worker instance mapping to provide future mapping between
    // worker messages and the bridge callbacks
    m_worker_instance_mapping.insert({ _client_hash, worker_allocation_result.value()});

    return true;
}

//
//
//

bool Jani::Runtime::OnWorkerLogMessage(
    WorkerId       _worker_id,
    WorkerLogLevel _log_level,
    std::string    _log_title,
    std::string    _log_message)
{
    std::cout << "[" << magic_enum::enum_name(_log_level) << "] " << _log_title << ": " << _log_message << std::endl;

    return true;
}

std::optional<Jani::EntityId> Jani::Runtime::OnWorkerReserveEntityIdRange(
    WorkerId _worker_id,
    uint32_t _total_ids)
{
    return m_database.ReserveEntityIdRange(_total_ids);
}

bool Jani::Runtime::OnWorkerAddEntity(
    WorkerId             _worker_id,
    EntityId             _entity_id,
    const EntityPayload& _entity_payload)
{
    // Make sure there is not active entity with the given id
    if (m_active_entities.find(_entity_id) != m_active_entities.end())
    {
        return false;
    }

    // Create the new entity on the database
    if (!m_database.AddEntity(_entity_id, _entity_payload))
    {
        return false;
    }

    // Create the new entity
    auto& entity = m_active_entities.insert({ _entity_id, Entity(_entity_id) }).first->second;

    // Add each component contained on the payload
    for (auto& component_payload : _entity_payload.component_payloads)
    {
        entity.AddComponent(component_payload.component_id);
    }

    return true;
}

bool Jani::Runtime::OnWorkerRemoveEntity(
    WorkerId _worker_id,
    EntityId _entity_id)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return false;
    }

    auto& entity = entity_iter->second;

    if (!m_database.RemoveEntity(_entity_id))
    {
        return false;
    }

    return true;
}

bool Jani::Runtime::OnWorkerAddComponent(
    WorkerId                _worker_id,
    EntityId                _entity_id,
    ComponentId             _component_id,
    const ComponentPayload& _component_payload)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return false;
    }

    auto& entity = entity_iter->second;

    // Check if this entity already has the given component id
    if (entity.HasComponent(_component_id))
    {
        return false;
    }

    // Register this component on the database for the given entity
    if (!m_database.AddComponent(_entity_id, _component_id, _component_payload))
    {
        return false;
    }

    // Add the component to the entity
    entity.AddComponent(_component_id);

    return true;
}

bool Jani::Runtime::OnWorkerRemoveComponent(
    WorkerId    _worker_id,
    EntityId    _entity_id,
    ComponentId _component_id)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return false;
    }

    auto& entity = entity_iter->second;

    // Check if this entity has the given component id
    if (!entity.HasComponent(_component_id))
    {
        return false;
    }

    // Unregister this component on the database for the given entity
    if (!m_database.RemoveComponent(_entity_id, _component_id))
    {
        return false;
    }

    // Remove the component from the entity
    entity.RemoveComponent(_component_id);

    return true;
}

bool Jani::Runtime::OnWorkerComponentUpdate(
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
        return false;
    }

    auto& entity = entity_iter->second;

    // Check if this entity already has the given component id
    if (entity.HasComponent(_component_id))
    {
        return false;
    }

    // Update this component on the database for the given entity
    if (!m_database.ComponentUpdate(_entity_id, _component_id, _component_payload, _entity_world_position))
    {
        return false;
    }

    return true;
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

    for (auto& [layer_hash, layer_info] : registered_layers)
    {
        if (!layer_info.is_user_layer)
        {
            // Make sure we have one bridge that owns managing this layer
            auto bridge_iter = m_bridges.find(layer_hash);
            if (bridge_iter == m_bridges.end())
            {
                auto new_bridge = std::make_unique<Bridge>(*this, layer_hash);
                m_bridges.insert({ layer_hash , std::move(new_bridge) });
                bridge_iter = m_bridges.find(layer_hash);
            }

            auto& bridge = bridge_iter->second;

            // We must have at least one worker on this bridge
            if (bridge->GetTotalWorkerCount() == 0 && m_worker_spawner_instances.size() > 0)
            {
                if (!m_worker_spawner_instances.back()->RequestWorkerForLayer(layer_hash))
                {
                    std::cout << "Unable to request a new worker from worker spawner, layer{" << layer_info.name << "}" << std::endl;
                }
            }
        }
    }

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