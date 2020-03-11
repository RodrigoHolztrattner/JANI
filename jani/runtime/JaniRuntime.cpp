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
#include "JaniWorldController.h"

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

    m_world_controller = std::make_unique<WorldController>(m_deployment_config, m_layer_collection);
    if (!m_world_controller->Initialize())
    {
        return false;
    }

    m_world_controller->RegisterCellOwnershipChangeCallback(
        [&](const std::map<EntityId, Entity*>& _entities, WorldCellCoordinates _cell_coordinates, LayerId _layer_id, const WorkerInstance& _current_worker, const WorkerInstance& _new_worker)
        {
            auto& layer_info = m_layer_collection.GetLayerInfo(_layer_id);

            std::cout << "Runtime -> Cell migration performed from worker_id{" << _current_worker.GetId() << "} to worker_id{" << _new_worker.GetId() << "}" << std::endl;

            for (auto& [entity_id, entity] : _entities)
            {
                Message::WorkerLayerAuthorityLostRequest authority_lost_request;
                authority_lost_request.entity_id = entity_id;

                if (!m_request_manager->MakeRequest(
                    *m_worker_connections,
                    _current_worker.GetConnectionClientHash(),
                    RequestType::WorkerLayerAuthorityLost,
                    authority_lost_request))
                {
                }

                Message::WorkerLayerAuthorityGainRequest authority_gain_request;
                authority_gain_request.entity_id = entity_id;

                if (!m_request_manager->MakeRequest(
                    *m_worker_connections,
                    _new_worker.GetConnectionClientHash(),
                    RequestType::WorkerLayerAuthorityGain,
                    authority_gain_request))
                {
                }

                for (auto component_id : layer_info.components)
                {
                    if (!entity->HasComponent(component_id))
                    {
                        continue;
                    }

                    auto component_payload = entity->GetComponentPayload(component_id);
                    if (!component_payload)
                    {
                        std::cout << "Runtime -> Error retrieving component payload for ownership transfer!" << std::endl;
                        continue;
                    }

                    Message::WorkerAddComponentRequest add_component_request;
                    add_component_request.entity_id         = entity->GetId();
                    add_component_request.component_id      = component_id;
                    add_component_request.component_payload = std::move(component_payload.value());

                    if (!m_request_manager->MakeRequest(
                        *m_worker_connections,
                        _new_worker.GetConnectionClientHash(),
                        RequestType::WorkerAddComponent,
                        add_component_request))
                    {
                    }
                }
            }
        });

    m_world_controller->RegisterEntityLayerOwnershipChangeCallback(
        [&](const Entity& _entity, LayerId _layer_id, const WorkerInstance& _current_worker, const WorkerInstance& _new_worker)
        {
            auto& layer_info = m_layer_collection.GetLayerInfo(_layer_id);

            Message::WorkerLayerAuthorityLostRequest authority_lost_request;
            authority_lost_request.entity_id = _entity.GetId();

            if (!m_request_manager->MakeRequest(
                *m_worker_connections,
                _current_worker.GetConnectionClientHash(),
                RequestType::WorkerLayerAuthorityLost,
                authority_lost_request))
            {
            }

            Message::WorkerLayerAuthorityGainRequest authority_gain_request;
            authority_gain_request.entity_id = _entity.GetId();

            if (!m_request_manager->MakeRequest(
                *m_worker_connections,
                _new_worker.GetConnectionClientHash(),
                RequestType::WorkerLayerAuthorityGain,
                authority_gain_request))
            {
            }

            for (auto component_id : layer_info.components)
            {
                if (!_entity.HasComponent(component_id))
                {
                    continue;
                }

                auto component_payload = _entity.GetComponentPayload(component_id);
                if (!component_payload)
                {
                    std::cout << "Runtime -> Error retrieving component payload for ownership transfer!" << std::endl;
                    continue;
                }

                // std::cout << "Runtime -> Entity crossed cell border entity_id{" << _entity.GetId() << "}, layer_id{" << _layer_id << "}" << std::endl;

                Message::WorkerAddComponentRequest add_component_request;
                add_component_request.entity_id         = _entity.GetId();
                add_component_request.component_id      = component_id;
                add_component_request.component_payload = std::move(component_payload.value());

                if (!m_request_manager->MakeRequest(
                    *m_worker_connections,
                    _new_worker.GetConnectionClientHash(),
                    RequestType::WorkerAddComponent,
                    add_component_request))
                {
                }
            }
        });

    m_world_controller->RegisterWorkerLayerRequestCallback(
        [&](LayerId _layer_id)
        {
            bool is_expecting_worker_for_layer = false;
            for (auto& worker_spawner : m_worker_spawner_instances)
            {
                if (worker_spawner->IsExpectingWorkerForLayer(_layer_id))
                {
                    is_expecting_worker_for_layer = true;
                    break;
                }
            }

            if (!is_expecting_worker_for_layer && !m_worker_spawner_instances.back()->RequestWorkerForLayer(_layer_id))
            {
                auto& registered_layers = m_layer_collection.GetLayers();
                std::cout << "Unable to request a new worker from worker spawner, layer{" << registered_layers.find(_layer_id)->second.name << "}" << std::endl;
            }
        });

    return true;
}

void Jani::Runtime::Update()
{
    m_client_connections->Update();
    m_worker_connections->Update();
    m_inspector_connections->Update();

    m_world_controller->Update();

    auto ProcessWorkerRequest = [&](
        auto                  _client_hash, 
        bool                  _is_user, 
        const RequestInfo&    _request, 
        const RequestPayload& _request_payload, 
        ResponsePayload&      _response_payload)
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
            auto authentication_request = _request_payload.GetRequest<Message::RuntimeClientAuthenticationRequest>();

            // Authenticate this worker
            // ...

            bool worker_allocation_result = TryAllocateNewWorker(
                Hasher(authentication_request.layer_name),
                _client_hash.value(),
                true);

            std::cout << "Runtime -> New client worker connected" << std::endl;

            Message::RuntimeClientAuthenticationResponse authentication_response = { worker_allocation_result };
            {
                _response_payload.PushResponse(std::move(authentication_response));
            }
        }
        // [[unlikely]]
        else if (_request.type == RequestType::RuntimeAuthentication)
        {
            auto authentication_request = _request_payload.GetRequest<Message::RuntimeAuthenticationRequest>();

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
                _response_payload.PushResponse(std::move(authentication_response));
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
        [](auto _client_hash, const RequestInfo& _request, const ResponsePayload& _response_payload)
        {
            // Currently we don't read from worker responses
        }, 
        [&](auto _client_hash, const RequestInfo& _request, const RequestPayload& _request_payload, ResponsePayload& _response_payload)
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
        [](auto _client_hash, const RequestInfo& _request, const RequestPayload& _request_payload)
        {
            // Currently we don't read from worker responses
        },
        [&](auto _client_hash, const RequestInfo& _request, const RequestPayload& _request_payload, ResponsePayload& _response_payload)
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
        [](auto _client_hash, const RequestInfo& _request, const ResponsePayload& _response_payload)
        {
            // Currently we don't read from inspector responses
        },
        [&](auto _client_hash, const RequestInfo& _request, const RequestPayload& _request_payload, ResponsePayload& _response_payload)
        {
            // Since we are always the server, there is no option for the client hash to be invalid
            if (!_client_hash)
            {
                return;
            }

            if (_request.type == RequestType::RuntimeGetEntitiesInfo)
            {
                auto get_entities_info_request = _request_payload.GetRequest<Message::RuntimeGetEntitiesInfoRequest>();

                Message::RuntimeGetEntitiesInfoResponse get_entities_info_response;
                get_entities_info_response.succeed = true;

                auto& entity_map = m_database.GetEntities();
                for (auto& [entity_id, entity] : entity_map)
                {
                    auto& cell_info = m_world_controller->GetWorldCellInfo(entity->GetWorldCellCoordinates());
                    
                    // Check if the message is getting too big and break it
                    if (get_entities_info_response.entities_infos.size() * sizeof(Message::RuntimeGetEntitiesInfoResponse::EntityInfo) > 500)
                    {
                        _response_payload.PushResponse(get_entities_info_response);
                        get_entities_info_response.entities_infos.clear();
                    }

                    get_entities_info_response.entities_infos.push_back({
                        entity_id,
                        entity->GetWorldPosition(),
                        cell_info.GetWorkerForLayer(0).value()->GetId(),
                        entity->GetComponentMask() });
                }

                _response_payload.PushResponse(std::move(get_entities_info_response));
                
            }
            else if (_request.type == RequestType::RuntimeGetCellsInfos)
            {
                auto get_cells_infos_request = _request_payload.GetRequest<Message::RuntimeGetCellsInfosRequest>();

                Message::RuntimeGetCellsInfosResponse get_cells_infos_response;
                get_cells_infos_response.succeed = true;

                auto& workers_infos_position_layer = m_world_controller->GetWorkersInfosForLayer(0);
                int32_t worker_length              = m_deployment_config.GetWorkerLength();
                for (auto& [worker_id, worker_info] : workers_infos_position_layer)
                {
                    for (auto& worker_coordinate : worker_info.worker_cells_infos.coordinates_owned)
                    {
                        // Check if the message is getting too big and break it
                        if (get_cells_infos_response.cells_infos.size() * sizeof(Message::RuntimeGetCellsInfosResponse::CellInfo) > 500)
                        {
                            _response_payload.PushResponse(get_cells_infos_response);
                            get_cells_infos_response.cells_infos.clear();
                        }

                        auto& cell_info           = m_world_controller->GetWorldCellInfo(worker_coordinate);
                        auto  cell_world_position = m_world_controller->ConvertCellCoordinatesIntoPosition(worker_coordinate);
                        auto  cell_rect           = WorldRect({ cell_world_position.x, cell_world_position.y, worker_length, worker_length });

                        get_cells_infos_response.cells_infos.push_back({
                            worker_id,
                            0,
                            cell_rect,
                            worker_coordinate, 
                            cell_info.entities.size() });
                    }
                }

                _response_payload.PushResponse(std::move(get_cells_infos_response));
            }
            else if (_request.type == RequestType::RuntimeGetWorkersInfos)
            {
                auto get_workerss_infos_request = _request_payload.GetRequest<Message::RuntimeGetWorkersInfosRequest>();

                Message::RuntimeGetWorkersInfosResponse get_workers_infos_response;
                get_workers_infos_response.succeed = true;

                for (int i = 0; i < MaximumLayers; i++)
                {
                    auto& workers_infos = m_world_controller->GetWorkersInfosForLayer(i);
                    for (auto& [worker_id, worker_info] : workers_infos)
                    {
                        // Check if the message is getting too big and break it
                        if (get_workers_infos_response.workers_infos.size() * sizeof(Message::RuntimeGetWorkersInfosResponse::WorkerInfo) > 500)
                        {
                            _response_payload.PushResponse(get_workers_infos_response);
                            get_workers_infos_response.workers_infos.clear();
                        }

                        get_workers_infos_response.workers_infos.push_back({
                        worker_id,
                        worker_info.worker_cells_infos.entity_count,
                        i,
                        worker_info.worker_instance->GetNetworkTrafficPerSecond().first, 
                        worker_info.worker_instance->GetNetworkTrafficPerSecond().second, 
                        0, 
                        0});
                    }

                }

                _response_payload.PushResponse(std::move(get_workers_infos_response));
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

    if (!m_layer_collection.HasLayer(_layer_id))
    {
        return false;
    }

    auto layer_info = m_layer_collection.GetLayerInfo(_layer_id);

    // Make sure the layer is an user layer if the requester is also an user, also vice-versa
    if (layer_info.user_layer != _is_user)
    {
        return false;
    }

    // Make sure that this layer support at least one worker
    if (layer_info.maximum_workers == 0)
    {
        return false;
    }

    // Check other layer requirements, as necessary
    // ...

    // Put a reference of this worker into the bridge, or make the worker knows what bridge it should reference?
    // worker_instance_ptr

    // Ok we are free to at least attempt to retrieve (or create) a bridge for this layer //
    auto bridge_iter = m_bridges.find(_layer_id);
    if (bridge_iter == m_bridges.end())
    {
        auto new_bridge = std::make_unique<Bridge>(*this, _layer_id);
        m_bridges.insert({ _layer_id , std::move(new_bridge) });
        bridge_iter = m_bridges.find(_layer_id);
    }

    auto& bridge = bridge_iter->second;

    auto worker_instance = std::make_unique<WorkerInstance>(
        *bridge,
        _layer_id,
        _client_hash,
        _is_user);

    WorkerInstance* worker_instance_ptr = worker_instance.get();

    if (!m_world_controller->AddWorkerForLayer(std::move(worker_instance), _layer_id))
    {
        return false;
    }

    // Add an entry into our worker instance mapping to provide future mapping between
    // worker messages and the bridge callbacks
    m_worker_instance_mapping.insert({ _client_hash, worker_instance_ptr });

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

    m_world_controller->InsertEntity(*entity.value(), entity.value()->GetWorldPosition());

    // Setup layer workers authority gain
    {
        for (int i = 0; i < MaximumLayers; i++)
        {
            auto worker = entity.value()->GetWorldCellInfo().GetWorkerForLayer(i);
            if (worker)
            {
                Message::WorkerLayerAuthorityGainRequest authority_gain_request;
                authority_gain_request.entity_id = _entity_id;

                if (!m_request_manager->MakeRequest(
                    *m_worker_connections,
                    worker.value()->GetConnectionClientHash(),
                    RequestType::WorkerLayerAuthorityGain,
                    authority_gain_request))
                {
                }
            }
        }
    }

    std::array<bool, MaximumLayers> worker_authority_control = {};
    for (auto requested_component_payload : _entity_payload.component_payloads)
    {
        LayerId layer_id = m_layer_collection.GetLayerIdForComponent(requested_component_payload.component_id);
        auto worker      = entity.value()->GetWorldCellInfo().GetWorkerForLayer(layer_id);
        if (!worker)
        {
            m_world_controller->RemoveEntity(*entity.value());

            // Remove the entity since the entire operation failed
            m_database.RemoveEntity(_worker_id, _entity_id);

            return false;
        }

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

    /*
    // Setup layer workers authority gain
    {
        for (int i = 0; i < MaximumLayers; i++)
        {
            LayerId layer_id = m_layer_collection.GetLayerIdForComponent(i);
            auto worker      = entity.value()->GetWorldCellInfo().GetWorkerForLayer(layer_id);
            if (worker)
            {
                Message::WorkerLayerAuthorityGainRequest authority_gain_request;
                authority_gain_request.entity_id = _entity_id;

                if (!m_request_manager->MakeRequest(
                    *m_worker_connections,
                    worker.value()->GetConnectionClientHash(),
                    RequestType::WorkerLayerAuthorityGain,
                    authority_gain_request))
                {
                }
            }
        }
    }
    */

    return true;
}

bool Jani::Runtime::OnWorkerAddComponent(
    WorkerInstance&         _worker_instance,
    WorkerId                _worker_id,
    EntityId                _entity_id,
    ComponentId             _component_id,
    const ComponentPayload& _component_payload)
{
    LayerId layer_id = m_layer_collection.GetLayerIdForComponent(_component_id);

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
        LayerId layer_id = m_layer_collection.GetLayerIdForComponent(_component_id);
        auto worker      = entity.value()->GetWorldCellInfo().GetWorkerForLayer(layer_id);
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
    }

    return true;
}

bool Jani::Runtime::OnWorkerRemoveComponent(
    WorkerInstance& _worker_instance,
    WorkerId        _worker_id,
    EntityId        _entity_id,
    ComponentId     _component_id)
{
    LayerId layer_id = m_layer_collection.GetLayerIdForComponent(_component_id);

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
    LayerId layer_id = m_layer_collection.GetLayerIdForComponent(_component_id);

    {
        auto entity = m_database.GetEntityById(_entity_id);
        if (!entity)
        {
            return false;
        }

        auto& cell_info  = m_world_controller->GetWorldCellInfo(entity.value()->GetWorldCellCoordinates());
        auto cell_worker = cell_info.GetWorkerForLayer(layer_id);
        if (!cell_worker)
        {
            return false;
        }

        // This can happen if we just changed the owned of an entity layer and it didn't received the message yet or
        // it arrived before it sent an update
        if (_worker_id != cell_worker.value()->GetId())
        {
            return false;
        }
    }

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
        entity.value()->SetWorldPosition(_entity_world_position.value(), _worker_id);
        m_world_controller->AcknowledgeEntityPositionChange(*entity.value(), _entity_world_position.value());
    }

    return true;
}

bool Jani::Runtime::OnWorkerComponentInterestQueryUpdate(
    WorkerInstance&                    _worker_instance,
    WorkerId                           _worker_id,
    EntityId                           _entity_id,
    ComponentId                        _component_id,
    const std::vector<ComponentQuery>& _component_queries)
{
    auto entity = m_database.GetEntityByIdMutable(_entity_id);
    if (!entity)
    {
        return false;
    }

    auto& cell_info  = m_world_controller->GetWorldCellInfo(entity.value()->GetWorldCellCoordinates());
    auto cell_worker = cell_info.GetWorkerForLayer(m_layer_collection.GetLayerIdForComponent(_component_id));
    if (!cell_worker)
    {
        return false;
    }

    // This can happen if we just changed the owned of an entity layer and it didn't received the message yet or
    // it arrived before it sent an update
    if (_worker_id != cell_worker.value()->GetId())
    {
        return false;
    }

    entity.value()->UpdateQueriesForComponent(_component_id, _component_queries);

    return true;
}
    
std::vector<Jani::ComponentPayload> Jani::Runtime::OnWorkerComponentInterestQuery(
    WorkerInstance&                    _worker_instance,
    WorkerId                           _worker_id,
    EntityId                           _entity_id,
    ComponentId                        _component_id)
{
    std::vector<ComponentPayload> components_payloads;
    
    auto entity = m_database.GetEntityByIdMutable(_entity_id);
    if (!entity)
    {
        return std::move(components_payloads);
    }

    auto& cell_info  = m_world_controller->GetWorldCellInfo(entity.value()->GetWorldCellCoordinates());
    auto cell_worker = cell_info.GetWorkerForLayer(m_layer_collection.GetLayerIdForComponent(_component_id));
    if (!cell_worker)
    {
        return std::move(components_payloads);
    }

    // This can happen if we just changed the owned of an entity layer and it didn't received the message yet or
    // it arrived before it sent an update
    if (_worker_id != cell_worker.value()->GetId())
    {
        return std::move(components_payloads);
    }
    
    // Get and apply the queries
    auto component_queries = entity.value()->GetQueriesForComponent(_component_id);
    for (auto& component_query : component_queries)
    {
        assert(component_query.query_owner_component == _component_id);

        std::unordered_map<EntityId, Entity*> selected_entities;

        int query_index = 0;
        for (auto& query : component_query.queries)
        {
            // Check if this is a combination 
            if (query.and_constraint || query.or_constraint)
            {
                assert(query_index > 0);

                if (query.and_constraint)
                {
                    // Not implemented for now
                }
                else
                {
                    // Not implemented for now
                }
            }
            else if (query.radius_constraint)
            {
                m_world_controller->ForEachEntityOnRadius(
                    entity.value()->GetWorldPosition(),
                    query.radius_constraint.value(),
                    [&](EntityId _selected_entity_id, Entity& _selected_entity, WorldCellCoordinates _cell_coordinates)
                    {
                        auto total_required_components = query.component_constraints_mask.count();
                        auto component_mask            = _selected_entity.GetComponentMask();
                        if ((query.component_constraints_mask & component_mask).count() == total_required_components)
                        {
                            selected_entities.insert({ _selected_entity_id, &_selected_entity });
                        }
                    });
            }
            else if (query.area_constraint)
            {
                // Not implemented for now
            }
            else
            {
                // Should not be used for now (until there is a better way to perform this type of query)
            }

            query_index++;
        }

        for (auto& [entity_id, entity] : selected_entities)
        {
            for (auto& requested_component_id : component_query.query_component_ids)
            {
                // Check if we should ignore this
                LayerId component_layer             = m_layer_collection.GetLayerIdForComponent(requested_component_id);
                auto current_component_layer_worker = m_world_controller->GetWorldCellInfo(entity->GetWorldCellCoordinates()).GetWorkerForLayer(component_layer);
                if (current_component_layer_worker && current_component_layer_worker.value()->GetId() == _worker_id)
                {
                    continue;
                }

                if (entity->HasComponent(requested_component_id))
                {
                    components_payloads.push_back(entity->GetComponentPayload(requested_component_id).value());
                }
            }
        }
    }

    return std::move(components_payloads);
}

bool Jani::Runtime::IsLayerForComponentAvailable(ComponentId _component_id) const
{
    // Convert the component id to its operating layer id
    LayerId layer_id = m_layer_collection.GetLayerIdForComponent(_component_id);

    for (auto& [bridge_layer_id, bridge] : m_bridges)
    {
        if (bridge_layer_id == layer_id)
        {
            return true;
        }
    }

    return false;
}