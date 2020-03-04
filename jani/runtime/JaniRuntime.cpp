////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntime.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntime.h"
#include "JaniDeploymentConfig.h"
#include "JaniLayerCollection.h"
#include "JaniWorkerSpawnerCollection.h"
#include "JaniBridge.h"
#include "JaniDatabase.h"
#include "JaniWorkerInstance.h"
#include "JaniWorkerSpawnerInstance.h"

const char* s_local_ip                  = "127.0.0.1";
uint32_t    s_client_worker_listen_port = 8080;
uint32_t    s_worker_listen_port        = 13051;
uint32_t    s_inspector_listen_port     = 14051;

Jani::Runtime::Runtime(
    Database&                      _database, 
    const DeploymentConfig&        _deployment_config, 
    const LayerCollection&         _layer_collection,
    const WorkerSpawnerCollection& _worker_spawner_collection) 
    : m_database(_database)
    , m_deployment_config(_deployment_config)
    , m_layer_collection(_layer_collection)
    , m_worker_spawner_collection(_worker_spawner_collection)
{
}

Jani::Runtime::~Runtime()
{
}

bool Jani::Runtime::Initialize()
{
    m_client_connections    = std::make_unique<Connection<>>(s_client_worker_listen_port);
    m_worker_connections    = std::make_unique<Connection<>>(s_worker_listen_port);
    m_inspector_connections = std::make_unique<Connection<>>(s_inspector_listen_port);
    m_request_manager       = std::make_unique<RequestManager>();

    auto& worker_spawners = m_worker_spawner_collection.GetWorkerSpawnersInfos();
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
    m_inspector_connections->Update();

    auto ProcessWorkerRequest = [&](
        auto                         _client_hash, 
        bool                         _is_user, 
        const Request&               _request, 
        cereal::BinaryInputArchive&  _request_payload, 
        cereal::BinaryOutputArchive& _response_payload)
    {
        // Since we are always the server, there is no option for the client hash to be invalid
        if (!_client_hash)
        {
            return;
        }

        // If this is an authentication request, process it, else delegate the request to the worker itself
        // [[unlikely]]
        if (_request.type == RequestType::RuntimeClientAuthentication)
        {
            Message::RuntimeClientAuthenticationRequest authentication_request;
            {
                _request_payload(authentication_request);
            }

            // Authenticate this worker
            // ...

            bool worker_allocation_result = TryAllocateNewWorker(
                Hasher(authentication_request.layer_name),
                _client_hash.value(),
                true);

            std::cout << "Runtime -> New client worker connected" << std::endl;

            Message::RuntimeClientAuthenticationResponse authentication_response = { worker_allocation_result };
            {
                _response_payload(authentication_response);
            }
        }
        // [[unlikely]]
        else if (_request.type == RequestType::RuntimeAuthentication)
        {
            Message::RuntimeAuthenticationRequest authentication_request;
            {
                _request_payload(authentication_request);
            }

            // Authenticate this worker
            // ...
            
            bool worker_allocation_result = TryAllocateNewWorker(
                authentication_request.layer_id,
                _client_hash.value(),
                false);

            if (worker_allocation_result)
            {
                for (auto& worker_spawner : m_worker_spawner_instances)
                {
                    worker_spawner->AcknowledgeWorkerSpawn(authentication_request.layer_id);
                }
            }

            std::cout << "Runtime -> New worker connected" << std::endl;

            Message::RuntimeAuthenticationResponse authentication_response = { worker_allocation_result, true, 7 };
            {
                _response_payload(authentication_response);
            }
        }
        else
        {
            auto worker_instance_iter = m_worker_instance_mapping.find(_client_hash.value());
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

    m_request_manager->Update(
        *m_client_connections, 
        [](auto _client_hash, const Request& _request, const RequestResponse& _response)
        {
            // Currently we don't read from worker responses
        }, 
        [&](auto _client_hash, const Request& _request, cereal::BinaryInputArchive& _request_payload, cereal::BinaryOutputArchive& _response_payload)
        {
            ProcessWorkerRequest(
                _client_hash,
                true,
                _request,
                _request_payload,
                _response_payload);
        });

    m_request_manager->Update(
        *m_worker_connections,
        [](auto _client_hash, const Request& _request, const RequestResponse& _response)
        {
            // Currently we don't read from worker responses
        },
        [&](auto _client_hash, const Request& _request, cereal::BinaryInputArchive& _request_payload, cereal::BinaryOutputArchive& _response_payload)
        {
            ProcessWorkerRequest(
                _client_hash,
                false,
                _request,
                _request_payload,
                _response_payload);
        });

    m_request_manager->Update(
        *m_inspector_connections,
        [](auto _client_hash, const Request& _request, const RequestResponse& _response)
        {
            // Currently we don't read from inspector responses
        },
        [&](auto _client_hash, const Request& _request, cereal::BinaryInputArchive& _request_payload, cereal::BinaryOutputArchive& _response_payload)
        {
            // Since we are always the server, there is no option for the client hash to be invalid
            if (!_client_hash)
            {
                return;
            }

            if (_request.type == RequestType::RuntimeGetEntitiesInfo)
            {
                Message::RuntimeGetEntitiesInfoRequest get_entities_info_request;
                {
                    _request_payload(get_entities_info_request);
                }

                Message::RuntimeGetEntitiesInfoResponse get_entities_info_response;
                get_entities_info_response.succeed = true;

                auto& entity_map = m_database.GetEntities();
                for (auto& [entity_id, entity] : entity_map)
                {
                    get_entities_info_response.entities_infos.push_back({
                        entity_id,
                        entity->GetRepresentativeWorldPosition(),
                        entity->GetRepresentativeWorldPositionWorker() });
                }

                {
                    _response_payload(get_entities_info_response);
                }
            }
            else if (_request.type == RequestType::RuntimeGetWorkersInfo)
            {
                Message::RuntimeGetWorkersInfoRequest get_workers_info_request;
                {
                    _request_payload(get_workers_info_request);
                }

                Message::RuntimeGetWorkersInfoResponse get_workers_info_response;
                get_workers_info_response.succeed = true;

                for (auto& [layer_id, bridge] : m_bridges)
                {
                    auto& workers = bridge->GetWorkers();
                    for (auto& [worker_id, worker_instance] : workers)
                    {
                        get_workers_info_response.worker_infos.push_back({
                            worker_id,
                            layer_id,
                            worker_instance->GetWorldRect(),
                            0 });
                    }
                }

                {
                    _response_payload(get_workers_info_response);
                }
            }
        });
    
    auto ProcessWorkerDisconnection = [&](auto _client_hash)
    {
        auto worker_instance_iter = m_worker_instance_mapping.find(_client_hash);
        if (worker_instance_iter == m_worker_instance_mapping.end())
        {
            return;
        }

        auto* worker_instance = worker_instance_iter->second;
        auto worker_layer_id  = worker_instance->GetLayerId();

        m_worker_instance_mapping.erase(worker_instance_iter);
        
        auto bridge_iter = m_bridges.find(worker_layer_id);
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

    m_inspector_connections->DidTimeout(
        [&](std::optional<Connection<>::ClientHash> _client_hash)
        {
            // Just let it timeout normally
        });

    for (auto& worker_spawner_connection : m_worker_spawner_instances)
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
}

bool Jani::Runtime::TryAllocateNewWorker(
    LayerId                  _layer_id,
    Connection<>::ClientHash _client_hash,
    bool                     _is_user)
{
    assert(m_worker_instance_mapping.find(_client_hash) == m_worker_instance_mapping.end());

    if (!m_layer_collection->HasLayer(_layer_id))
    {
        return false;
    }

    auto layer_info = m_layer_collection->GetLayerInfo(_layer_id);

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
    auto bridge_iter = m_bridges.find(_layer_id);
    if (bridge_iter == m_bridges.end())
    {
        auto new_bridge = std::make_unique<Bridge>(*this, _layer_id);
        m_bridges.insert({ _layer_id , std::move(new_bridge) });
        bridge_iter = m_bridges.find(_layer_id);
    }

    auto& bridge = bridge_iter->second;

    auto worker_allocation_result = bridge->TryAllocateNewWorker(_layer_id, _client_hash, _is_user);
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
    WorkerInstance& _worker_instance,
    WorkerId        _worker_id,
    WorkerLogLevel  _log_level,
    std::string     _log_title,
    std::string     _log_message)
{
    std::cout << "[" << magic_enum::enum_name(_log_level) << "] " << _log_title << ": " << _log_message << std::endl;

    return true;
}

std::optional<Jani::EntityId> Jani::Runtime::OnWorkerReserveEntityIdRange(
    WorkerInstance& _worker_instance,
    WorkerId        _worker_id,
    uint32_t        _total_ids)
{
    return m_database.ReserveEntityIdRange(_total_ids);
}

bool Jani::Runtime::OnWorkerAddEntity(
    WorkerInstance&      _worker_instance,
    WorkerId             _worker_id,
    EntityId             _entity_id,
    const EntityPayload& _entity_payload)
{
    // Quick check if there are layers available for all requested components
    for (auto requested_component_payload : _entity_payload.component_payloads)
    {
        if (!IsLayerForComponentAvailable(requested_component_payload.component_id))
        {
            return false;
        }
    }

    // Create the new entity on the database
    auto entity = m_database.AddEntity(_worker_id, _entity_id, _entity_payload);
    if (!entity)
    {
        return false;
    }

    for (auto requested_component_payload : _entity_payload.component_payloads)
    {
        auto worker = GetBestWorkerForComponent(requested_component_payload.component_id, entity.value()->GetRepresentativeWorldPosition());
        if (!worker)
        {
            // Remove the entity since the entire operation failed
            m_database.RemoveEntity(_worker_id, _entity_id);

            return false;
        }

        LayerId layer_id = m_layer_collection->GetLayerIdForComponent(requested_component_payload.component_id);

        Message::WorkerAddComponentRequest add_component_request;
        add_component_request.entity_id         = _entity_id;
        add_component_request.component_id      = requested_component_payload.component_id;
        add_component_request.component_payload = requested_component_payload;

        // Send a message to this worker to acknowledge him about receiving the component/entity
        if (!m_request_manager->MakeRequest(
            *m_worker_connections,
            worker.value()->GetConnectionClientHash(), 
            RequestType::WorkerAddComponent,
            add_component_request))
        {
            // Remove the entity since the entire operation failed
            m_database.RemoveEntity(_worker_id, _entity_id);

            return false;
        }
        
        entity.value()->SetComponentAuthority(
            layer_id,
            requested_component_payload.component_id, 
            worker.value()->GetId());
    }
    
    return true;
}

bool Jani::Runtime::OnWorkerRemoveEntity(
    WorkerInstance& _worker_instance,
    WorkerId        _worker_id,
    EntityId        _entity_id)
{
    if (!m_database.RemoveEntity(_worker_id, _entity_id))
    {
        return false;
    }

    return true;
}

bool Jani::Runtime::OnWorkerAddComponent(
    WorkerInstance&         _worker_instance,
    WorkerId                _worker_id,
    EntityId                _entity_id,
    ComponentId             _component_id,
    const ComponentPayload& _component_payload)
{
    LayerId layer_id = m_layer_collection->GetLayerIdForComponent(_component_id);

    // Quick check if there is a layer available to this component
    if (!IsLayerForComponentAvailable(_component_id))
    {
        return false;
    }

    // Register this component on the database for the given entity
    auto entity = m_database.AddComponent(
        _worker_id, 
        _entity_id, 
        layer_id,
        _component_id, 
        _component_payload);
    if (!entity)
    {
        return false;
    }

    {
        auto worker = GetBestWorkerForComponent(_component_id, entity.value()->GetRepresentativeWorldPosition());
        if (!worker)
        {
            m_database.RemoveComponent(
                _worker_id, 
                _entity_id, 
                layer_id, 
                _component_id);
            return false;
        }

        Message::WorkerAddComponentRequest add_component_request;
        add_component_request.entity_id         = _entity_id;
        add_component_request.component_id      = _component_id;
        add_component_request.component_payload = _component_payload;

        // Send a message to this worker to acknowledge him about receiving the component/entity
        if (!m_request_manager->MakeRequest(
            *m_worker_connections,
            worker.value()->GetConnectionClientHash(),
            RequestType::WorkerAddComponent,
            add_component_request))
        {
            m_database.RemoveComponent(
                _worker_id,
                _entity_id,
                layer_id, 
                _component_id);
            return false;
        }

        entity.value()->SetComponentAuthority(
            layer_id, 
            _component_id, 
            worker.value()->GetId());
    }

    return true;
}

bool Jani::Runtime::OnWorkerRemoveComponent(
    WorkerInstance& _worker_instance,
    WorkerId        _worker_id,
    EntityId        _entity_id,
    ComponentId     _component_id)
{
    LayerId layer_id = m_layer_collection->GetLayerIdForComponent(_component_id);

    // Get the worker owner for this component
    // ...

    // Send a message to remove the given component
    // ...

    // Unregister this component on the database for the given entity
    auto entity = m_database.RemoveComponent(
        _worker_id, 
        _entity_id, 
        layer_id, 
        _component_id);
    if (!entity)
    {
        return false;
    }

    return true;
}

bool Jani::Runtime::OnWorkerComponentUpdate(
    WorkerInstance&              _worker_instance,
    WorkerId                     _worker_id,
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    const ComponentPayload&      _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    LayerId layer_id = m_layer_collection->GetLayerIdForComponent(_component_id);

    // Update this component on the database for the given entity
    auto entity = m_database.ComponentUpdate(
        _worker_id,
        _entity_id, 
        _component_id, 
        layer_id, 
        _component_payload,
        _entity_world_position);
    if (!entity)
    {
        return false;
    }

    if (_entity_world_position)
    {
        entity.value()->SetRepresentativeWorldPosition(_entity_world_position.value(), _worker_id);
    }

    return true;
}

Jani::WorkerRequestResult Jani::Runtime::OnWorkerComponentQuery(
    WorkerInstance&       _worker_instance,
    WorkerId              _worker_id,
    EntityId              _entity_id,
    const ComponentQuery& _component_query)
{
    auto entity_opt = m_database.GetEntityById(_entity_id);
    if (!entity_opt)
    {
        return WorkerRequestResult(false);

    }

#if 0
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
#endif

    // For each selected entity, get the requested components and prepare the return data
    // ...

    return WorkerRequestResult(false);
}

bool Jani::Runtime::IsLayerForComponentAvailable(ComponentId _component_id) const
{
    // Convert the component id to its operating layer id
    LayerId layer_id = m_layer_collection->GetLayerIdForComponent(_component_id);

    for (auto& [bridge_layer_id, bridge] : m_bridges)
    {
        if (bridge_layer_id == layer_id)
        {
            return true;
        }
    }

    return false;
}

std::optional<Jani::WorkerInstance*> Jani::Runtime::GetBestWorkerForComponent(
    ComponentId                  _component_id,
    std::optional<WorldPosition> _entity_world_position) const
{
    LayerId layer_id = m_layer_collection->GetLayerIdForComponent(_component_id);
    auto layer_iter  = m_bridges.find(layer_id);
    if (layer_iter != m_bridges.end())
    {
        for (auto& [worker_id, worker] : layer_iter->second->GetWorkers())
        {
            return worker.get();
        }
    }

    return std::nullopt;
}

void Jani::Runtime::ApplyLoadBalanceUpdate()
{
    auto&                           worker_spawners             = m_worker_spawner_collection->GetWorkerSpawnersInfos();
    auto&                           registered_layers           = m_layer_collection->GetLayers();
    std::array<bool, MaximumLayers> is_worker_required_on_layer = {};

    /*
        => O que essa funcao deve fazer:

        - Ela deve rodar tipo uma vez a cada segundo (nao muito frequente)

        - Deve verificar se existe pelo menos 1 layer de cada tipo ativado, caso nao tenha ele deve
        requisitar a criacao dos workers necessarios
        - Deve verificar dentre os layers/bridges existentes se alguma esta sobrecarregada, requisitando
        a criacao de um novo worker para o layer em questao
    */

    // Check for workers that are over their capacity
    // We only need to check the ones who use spatial area, if there is no
    // area limit we will evenly distribute the entities across workers
    // so no validation is needed
    for (auto& [layer_id, bridge] : m_bridges)
    {
        auto& worker_map = bridge->GetWorkers();
        for (auto& [worker_id, worker] : worker_map)
        {
            auto entities_on_area_limit = worker->GetEntitiesOnAreaLimit();
            std::array<std::optional<std::tuple<WorkerInstance*, uint32_t, Entity*>>, 4> distance_infos;

            // Check if there are enough entities on this worker for us to consider moving them
            // to another one
            // ...

            if (!worker->IsOverCapacity()/* || !worker->UseSpatialArea()*/)
            {
                continue;
            }

            for (int i = 0; i < entities_on_area_limit.size(); i++)
            {
                auto& entity_info = entities_on_area_limit[i];
                if (!entity_info)
                {
                    continue;
                }

                auto entity = m_database.GetEntityByIdMutable(entity_info.value());
                if (!entity)
                {
                    continue;
                }

                /*
                * One optimization would be passing the entity position to the worker and not only the ids for
                * the extreme locations, that way we would not need to retrieve the entity from the database map 
                * all the time
                */
                WorldPosition entity_position = entity.value()->GetRepresentativeWorldPosition();

                bool found_worker = false;
                for (auto& [thief_worker_id, thief_worker] : worker_map)
                {
                    if (worker_id != thief_worker_id && !thief_worker->IsOverCapacity())
                    {
                        uint32_t distance_to_worker_area = thief_worker->GetWorldRect().ManhattanDistanceFromPosition(entity_position);

                        if (!distance_infos[i] || (std::get<1>(distance_infos[i].value()) > distance_to_worker_area /* && distance_to_worker_area > 100 */))
                        {
                            distance_infos[i] = { thief_worker.get() , distance_to_worker_area, entity.value() };
                            found_worker = true;
                        }
                    }
                }

                if (!found_worker)
                {
                    if (!is_worker_required_on_layer[layer_id])
                    {
                        std::cout << "Runtime -> Requesting to create another worker since one is over capacity and there is no other available to steal components from it" << std::endl;
                    }

                    is_worker_required_on_layer[layer_id] = true;
                }
            }

            for (auto& distance_info : distance_infos)
            {
                if (!distance_info)
                {
                    continue;
                }

                auto [thief_worker, thief_worker_distance, entity] = distance_info.value();

                auto position_component = entity->GetComponentPayload(0);
                if (!position_component)
                {
                    continue;
                }

                ComponentId component_id = 0; // This should be done for all components, not only position

                std::cout << "Runtime -> Moving a component from an overloaded worker to another entity_id{" << entity->GetId() << "}, component_id{" << component_id << "}" << std::endl;

                {
                    Message::WorkerAddComponentRequest add_component_request;
                    add_component_request.entity_id         = entity->GetId();
                    add_component_request.component_id      = component_id;
                    add_component_request.component_payload = std::move(position_component.value());

                    if (!m_request_manager->MakeRequest(
                        *m_worker_connections,
                        thief_worker->GetConnectionClientHash(),
                        RequestType::WorkerAddComponent,
                        add_component_request))
                    {

                    }

                    entity->SetComponentAuthority(
                        layer_id,
                        component_id,
                        thief_worker->GetId());
                }

                {
                    Message::WorkerRemoveComponentRequest remove_component_request;
                    remove_component_request.entity_id    = entity->GetId();
                    remove_component_request.component_id = component_id;

                    if (!m_request_manager->MakeRequest(
                        *m_worker_connections,
                        worker->GetConnectionClientHash(),
                        RequestType::WorkerRemoveComponent,
                        remove_component_request))
                    {

                    }
                }
            }
        }

        for (auto& [worker_id, worker] : worker_map)
        {
            worker->ResetOverCapacityFlag();
        }
    }

    for (auto& [layer_id, layer_info] : registered_layers)
    {
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
            if (bridge->GetTotalWorkerCount() == 0 && m_worker_spawner_instances.size() > 0)
            {
                is_worker_required_on_layer[layer_id] = true;
            }
        }
    }
    
    for (int i = 0; i < is_worker_required_on_layer.size(); i++)
    {
        if (is_worker_required_on_layer[i])
        {
            bool is_expecting_worker_for_layer = false;
            for (auto& worker_spawner : m_worker_spawner_instances)
            {
                if (worker_spawner->IsExpectingWorkerForLayer(i))
                {
                    is_expecting_worker_for_layer = true;
                    break;
                }
            }

            if (!is_expecting_worker_for_layer && !m_worker_spawner_instances.back()->RequestWorkerForLayer(i))
            {
                std::cout << "Unable to request a new worker from worker spawner, layer{" << registered_layers.find(i)->second.name << "}" << std::endl;
            }
        }
    }
}