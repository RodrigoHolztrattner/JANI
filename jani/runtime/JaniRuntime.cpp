////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntime.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntime.h"
#include "config/JaniDeploymentConfig.h"
#include "config/JaniLayerConfig.h"
#include "config/JaniWorkerSpawnerConfig.h"
#include "JaniRuntimeBridge.h"
#include "JaniRuntimeDatabase.h"
#include "JaniRuntimeWorkerReference.h"
#include "JaniRuntimeWorkerSpawnerReference.h"
#include "JaniRuntimeWorldController.h"

/*
    *** Pendencias 

    => New player storage object
        - Preciso criar um objeto que vai guardar entidades de jogadores, essas entidades devem ser
        salvas com um account/player ID e algum outro identificador (caso o jogador tenha mais de uma
        entidade salva no momento)
        - Devo ter como iterar sobre essas entidades e pedir para o Runtime criar ela(s) quando o cliente
        se connectar (eh isso mesmo? pelo que eu entendo o cliente pode abusar isso de varias formas, tipo
        escolhendo invalidamente logar usando um personagem que nao deveria, tipo um summon em vez da
        entidade principal)
        - Esse objeto deve funcionar como a main database persistent do jogo

        - Preciso passar um arquivo que contém uma lista de entidades que devem ser criadas com o mundo
        - Preciso passar um arquivo com as entidades do mundo organizadas por posição
        - Os dois arquivos acima podem compreender o snapshot
        - Adicionar flags na entity (se ela não pode mudar de cell, se ela deve ser salva no snapshot, se eh controlada por player?, etc)
        - Permitir que algo externo injete entidades que podem ou não serem linkadas a uma client worker connection (se forem, devem expirar depois de um tempo caso ele não conecte)

        ----------------------------------

        => Client View
        - Clientes podem ter ownership sobre certos componentes, dessa forma eu posso fazer com que o jogo "siga" a virtual position de
        uma entidade.
        - Para garantir que o cliente nao ira fazer algo indevido, outro worker pode mudar a qbi de um dos componentes que sao controlados
        pelo cliente a fim de fazer o cliente receber os updates sobre entidades em volta do mesmo

        => Client entity(s)
        - O cliente faz o login inicial, seleciona o seu personagem (ou cria o mesmo, enviando os dados para a database), ao realizar
        a preparacao para entrar no mundo do jogo, o runtime recebe a informacao sobre o personagem(s) do cliente, o cliente segue
        com a entrada no mundo, Runtime percebe que um client worker entrou e "acorda/cria" as entidades marcadas com aquele ID

        => Persistent World
        - Ao carregar uma area nova (cell), runtime deve requisitar info da database sobre todas as entidades que estao dentro dessa regiao
        e que devem ser criadas/carregadas
        - Caso essa regiao fique sem uso (sem nenhuma player entity por perto e sem nenhuma entidade que seja essencial presente), tal regiao
        pode ser removida da memoria, com suas entidades dinamicas sendo salvas (e possivelmente as static entities sendo ignoradas, afinal
        sao estaticas e sempre vao estar la)

        => Snapshot
        - Runtime (ou algo intermediario):
            + Precisa conter as informacoes de todas as entidades essenciais que devem ser carregadas com o startup do mundo
            + Precisa tambem ter a informacao de entidades localizadas em cada cell (permitir carregar as mesmas com o uso da cell)
            + Deve poder receber info sobre player entities e adicionar estas no mundo quando necessario

        - Database:
            + Deve conter as informacoes de todas entidades essenciais?! (runtime ou database?!)
            + Deve conter as informacoes de entidades dos jogadores (deve disponibilizar essa informacao para o runtime quando necessario)

        Perguntas:
            - Ao salvar o "mundo" devo criar 2 tipos de saves (static e dynamic/essential)?
            - Workers devem ter acesso ao mundo fisico (terrain) e nao sobre as entidades? O que eh responsabelidade de um worker
            e o que eh responsabelidade do runtime criar?
            - O runtime deve acessar um snapshot completo ou so com as entidades estaticas? Deve ser responsabelidade da database
            (ou outro sistema) de fornecer informacoes sobre entidades dinamicas/essenciais quando o mundo eh carregado?
            - O snapshot com entidades estaticas deve ser salvo localmente? No caso de uma entidade que nao eh necessariamente
            estatica, tipo um npc que se movimenta, caso ele mude de cell, eu preciso armazenar isso no snapshot, correto? (ou
            deveria ter algo que continua simulando a movimentacao desse npc "offline"?)

*/

Jani::Runtime::Runtime(
    RuntimeDatabase&           _database, 
    const DeploymentConfig&    _deployment_config, 
    const LayerConfig&         _layer_config,
    const WorkerSpawnerConfig& _worker_spawner_config) 
    : m_database(_database)
    , m_deployment_config(_deployment_config)
    , m_layer_config(_layer_config)
    , m_worker_spawner_config(_worker_spawner_config)
{
}

Jani::Runtime::~Runtime()
{
}

bool Jani::Runtime::Initialize()
{
    m_thread_pool = std::make_unique<WorkerPool>(m_deployment_config.GetThreadPoolSize());

    m_client_connections    = std::make_unique<Connection<>>(m_deployment_config.GetClientWorkerListenPort());
    m_worker_connections    = std::make_unique<Connection<>>(m_deployment_config.GetServerWorkerListenPort());
    m_inspector_connections = std::make_unique<Connection<>>(m_deployment_config.GetInspectorListenPort());
    m_request_manager       = std::make_unique<RequestManager>();

    auto& worker_spawners = m_worker_spawner_config.GetWorkerSpawnersInfos();
    for (auto& worker_spawner_info : worker_spawners)
    {
        static uint32_t spawner_port = 13000;

        auto spawner_connection = std::make_unique<RuntimeWorkerSpawnerReference>();

        if (!spawner_connection->Initialize(
            spawner_port++,
            worker_spawner_info.port,
            worker_spawner_info.ip.c_str(),
            m_deployment_config.GetServerWorkerListenPort(),
            m_deployment_config.GetRuntimeIp()))
        {
            return false;
        }

        m_worker_spawner_instances.push_back(std::move(spawner_connection));
    }

    m_world_controller = std::make_unique<RuntimeWorldController>(m_deployment_config, m_layer_config);
    if (!m_world_controller->Initialize())
    {
        return false;
    }

    m_world_controller->RegisterCellOwnershipChangeCallback(
        [&](const std::map<EntityId, ServerEntity*>& _entities, WorldCellCoordinates _cell_coordinates, LayerId _layer_id, const RuntimeWorkerReference* _current_worker, const RuntimeWorkerReference* _new_worker)
        {
            auto& layer_info = m_layer_config.GetLayerInfo(_layer_id);

            if (_current_worker == nullptr && _new_worker == nullptr)
            {
                return;
            }

            if (_current_worker != nullptr && _new_worker == nullptr)
            {
                Jani::MessageLog().Warning("Runtime -> Emergency moving entities from worker_id {} to the dummy worker on layer_id {} for cell ({},{})", _current_worker->GetId(), _layer_id, _cell_coordinates.x, _cell_coordinates.y);
            }
            else if (_current_worker == nullptr && _new_worker != nullptr)
            {
                Jani::MessageLog().Info("Runtime -> Moving entities from dummy worker to worker_id {} on layer_id {} for cell ({},{})", _new_worker->GetId(), _layer_id, _cell_coordinates.x, _cell_coordinates.y);
            }
            else
            {
                Jani::MessageLog().Info("Runtime -> Cell migration performed from worker_id {} to worker_id {} on layer_id {} for cell ({},{})", _current_worker->GetId(), _new_worker->GetId(), _layer_id, _cell_coordinates.x, _cell_coordinates.y);
            }

            for (auto& [entity_id, entity] : _entities)
            {
                Message::WorkerLayerAuthorityLostRequest authority_lost_request;
                authority_lost_request.entity_id = entity_id;

                if (_current_worker)
                {
                    if (!m_request_manager->MakeRequest(
                        *m_worker_connections,
                        _current_worker->GetConnectionClientHash(),
                        RequestType::WorkerLayerAuthorityLost,
                        authority_lost_request))
                    {
                    }
                }

                if (_new_worker)
                {
                    Message::WorkerLayerAuthorityGainRequest authority_gain_request;
                    authority_gain_request.entity_id = entity_id;

                    if (!m_request_manager->MakeRequest(
                        *m_worker_connections,
                        _new_worker->GetConnectionClientHash(),
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

                        Message::WorkerAddComponentRequest add_component_request;
                        add_component_request.entity_id         = entity->GetId();
                        add_component_request.component_id      = component_id;
                        add_component_request.component_payload = entity->GetComponentPayload(component_id); // If possible, somehow avoid this allocation!!

                        if (!m_request_manager->MakeRequest(
                            *m_worker_connections,
                            _new_worker->GetConnectionClientHash(),
                            RequestType::WorkerAddComponent,
                            add_component_request))
                        {
                        }
                    }
                }
            }
        });

    m_world_controller->RegisterEntityLayerOwnershipChangeCallback(
        [&](const ServerEntity& _entity, LayerId _layer_id, const RuntimeWorkerReference& _current_worker, const RuntimeWorkerReference& _new_worker)
        {
            auto& layer_info = m_layer_config.GetLayerInfo(_layer_id);

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

                Message::WorkerAddComponentRequest add_component_request;
                add_component_request.entity_id         = _entity.GetId();
                add_component_request.component_id      = component_id;
                add_component_request.component_payload = _entity.GetComponentPayload(component_id); // If possible, somehow avoid this allocation!!

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

            if (!is_expecting_worker_for_layer)
            {
                auto& registered_layers = m_layer_config.GetLayers();
                Jani::MessageLog().Info("Runtime -> Requesting new worker for layer {}", registered_layers.find(_layer_id)->second.name);
            }

            if (!is_expecting_worker_for_layer && !m_worker_spawner_instances.back()->RequestWorkerForLayer(_layer_id))
            {
                auto& registered_layers = m_layer_config.GetLayers();
                Jani::MessageLog().Error("Runtime -> Unable to request a new worker from worker spawner, layer {}", registered_layers.find(_layer_id)->second.name);
            }
        });

    return true;
}

void Jani::Runtime::Update()
{
    m_client_connections->Update();
    
    {
        // ElapsedTimeAutoLogger("Workers update: ", 1000);

        jobxx::job query_job = m_thread_pool->GetQueue().create_job([this](jobxx::context& ctx)
        {
            ctx.spawn_task([this]()
            {
                m_worker_connections->Update([](auto client_info_wrapper, auto _parallel_update_callback)
                {
                    _parallel_update_callback(client_info_wrapper);
                });
            });

        });

        m_thread_pool->GetQueue().wait_job_actively(query_job);
    }

    m_inspector_connections->Update();

    {
        // ElapsedTimeAutoLogger("World controller update: ", 1000);

        m_world_controller->Update();
    }
    
    {
        // ElapsedTimeAutoLogger("Total query time: ", 1000);

        jobxx::job query_job = m_thread_pool->GetQueue().create_job(
            [this](jobxx::context& ctx)
            {
                m_entity_query_controller.ForEach(
                [&](auto& query_info) -> void
                {
                    ctx.spawn_task(
                        [this, &query_info]()
                        {
                            EntityId    entity_id     = query_info.entity_id;
                            ComponentId component_id  = query_info.component_id;
                            uint32_t    query_version = query_info.query_version;

                            auto entity = m_database.GetEntityByIdMutable(entity_id);
                            if (!entity)
                            {
                                query_info.is_outdated = true;
                                return;
                            }

                            if (entity.value()->GetQueryVersion(component_id) != query_version)
                            {
                                query_info.is_outdated = true;
                                return;
                            }

                            auto& cell_info = m_world_controller->GetWorldCellInfo(entity.value()->GetWorldCellCoordinates());
                            auto cell_worker = cell_info.GetWorkerForLayer(m_layer_config.GetLayerIdForComponent(component_id));
                            if (!cell_worker)
                            {
                                return;
                            }

                            nonstd::transient_vector<std::pair<ComponentMask, nonstd::transient_vector<const ComponentPayload*>>> query_results;

                            // Get and apply the queries
                            auto component_queries = entity.value()->GetQueriesForComponent(component_id);
                            for (auto& component_query : component_queries)
                            {
                                auto query_result = PerformComponentQuery(component_query, entity.value()->GetWorldPosition(), cell_worker.value()->GetId());
                                query_results.insert(query_results.end(), std::make_move_iterator(query_result.begin()), std::make_move_iterator(query_result.end()));
                            }

                            {
                                for (auto& query_entry : query_results)
                                {
                                    Message::RuntimeComponentInterestQueryResponse response;
                                    response.succeed               = true;
                                    response.entity_component_mask = query_entry.first;
                                    response.components_payloads.reserve(query_entry.second.size());

                                    uint32_t total_accumulated_size = 0;
                                    for (auto& component_payload : query_entry.second)
                                    {
                                        // Check if the message is getting too big and break it
                                        if (total_accumulated_size + component_payload->component_data.size() > 500)
                                        {
                                            m_request_manager->MakeRequest(
                                                *m_worker_connections,
                                                cell_worker.value()->GetConnectionClientHash(), 
                                                Jani::RequestType::RuntimeComponentInterestQuery,
                                                response);

                                            response.components_payloads.clear();
                                            total_accumulated_size = 0;
                                        }

                                        response.components_payloads.push_back(*component_payload);

                                        total_accumulated_size += response.components_payloads.back().component_data.size();
                                    }

                                    if (response.components_payloads.size() > 0)
                                    {
                                        m_request_manager->MakeRequest(
                                            *m_worker_connections,
                                            cell_worker.value()->GetConnectionClientHash(),
                                            Jani::RequestType::RuntimeComponentInterestQuery,
                                            response);
                                    }
                                }
                            }
                        });
                });  
            });
        m_thread_pool->GetQueue().wait_job_actively(query_job);
    }

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
                WorkerType::Client);

            Jani::MessageLog().Info("Runtime -> New client worker connected");

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
                WorkerType::Server);

            if (worker_allocation_result)
            {
                Jani::MessageLog().Info("Runtime -> New worker connected with id {} for layer {}", _client_hash.value(), authentication_request.layer_id);

                for (auto& worker_spawner : m_worker_spawner_instances)
                {
                    worker_spawner->AcknowledgeWorkerSpawn(authentication_request.layer_id);
                }
            }

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

    {
        // ElapsedTimeAutoLogger("Workers requests: ", 1000);

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
    }

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
                        entity->GetComponentMask() });
                }

                _response_payload.PushResponse(std::move(get_entities_info_response));
                
            }
            else if (_request.type == RequestType::RuntimeGetCellsInfos)
            {
                auto get_cells_infos_request = _request_payload.GetRequest<Message::RuntimeGetCellsInfosRequest>();

                Message::RuntimeGetCellsInfosResponse get_cells_infos_response;
                get_cells_infos_response.succeed = true;

                auto& workers_infos_position_layer = m_world_controller->GetWorkersInfosForLayer(get_cells_infos_request.layer_id);
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
                            get_cells_infos_request.layer_id,
                            cell_rect,
                            worker_coordinate, 
                            static_cast<uint32_t>(cell_info.entities.size()) });
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
            else if (_request.type == RequestType::RuntimeInspectorQuery)
            {
                auto inspector_query_request = _request_payload.GetRequest<Message::RuntimeInspectorQueryRequest>();
                auto query_entries           = PerformComponentQuery(inspector_query_request.query, inspector_query_request.query_center_location);

                for (auto& query_entry : query_entries)
                {
                    if (query_entry.second.size() == 0)
                    {
                        continue;
                    }

                    Message::RuntimeInspectorQueryResponse response;
                    response.succeed               = true;
                    response.window_id             = inspector_query_request.window_id;
                    response.entity_id             = query_entry.second.front()->entity_owner;
                    response.entity_component_mask = query_entry.first;
                    response.entity_world_position = WorldPosition({ 0, 0 });
                    response.components_payloads.reserve(query_entry.second.size());

                    auto entity = m_database.GetEntityById(response.entity_id);
                    if (entity)
                    {
                        response.entity_world_position = entity.value()->GetWorldPosition();
                    }

                    uint32_t total_accumulated_size = 0;
                    for (auto& component_payload : query_entry.second)
                    {
                        // Check if the message is getting too big and break it
                        if (total_accumulated_size + component_payload->component_data.size() > 500)
                        {
                            _response_payload.PushResponse(response);
                            response.components_payloads.clear();
                            total_accumulated_size = 0;
                        }

                        response.components_payloads.push_back(*component_payload);

                        total_accumulated_size += response.components_payloads.back().component_data.size();
                    }

                    if (response.components_payloads.size() > 0)
                    {
                        _response_payload.PushResponse(std::move(response));
                    }
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
        WorkerId worker_id    = worker_instance->GetId();
        auto worker_layer_id  = worker_instance->GetLayerId();

        m_worker_instance_mapping.erase(worker_instance_iter);
        
        auto bridge_iter = m_bridges.find(worker_layer_id);
        if (bridge_iter == m_bridges.end())
        {
            return;
        }

        auto* bridge = bridge_iter->second.get();

        // There is no point on disconnecting the worker from the bridge since the one who owns the worker is the
        // world controller
        // This behavior may change in the future
        /*
        bool disconnect_result = bridge->DisconnectWorker(_client_hash);
        if (!disconnect_result)
        {
            Jani::MessageLog().Error("Runtime -> Problem trying to disconnect worker, its not registered on the bridge");
        }
        else
        {
            Jani::MessageLog().Info("Runtime -> Worker disconnected with id {} for layer {} with client hash {}", worker_id, worker_layer_id, _client_hash);
        }
        */

        if (!m_world_controller->HandleWorkerDisconnection(worker_id, worker_layer_id))
        {
            Jani::MessageLog().Critical("Runtime -> Worker disconnection not handled correctly by the world controller, this failure will trigger an emergency backup and shutdown since the runtime is not able to continue on a non-broken state, worker_id {}, worker_layer_id {}, worker_client_hash {}", worker_id, worker_layer_id, _client_hash);
            exit(11);
        }

        Jani::MessageLog().Info("Runtime -> Worker disconnected with id {} for layer {} with client hash {}", worker_id, worker_layer_id, _client_hash);
    };

    m_client_connections->DidTimeout(
        [&](std::optional<Connection<>::ClientHash> _client_hash)
        {
            assert(_client_hash.has_value());
            ProcessWorkerDisconnection(_client_hash.value());
        });

    {
        // ElapsedTimeAutoLogger("Workers timeout check: ", 1000);

        m_worker_connections->DidTimeout(
            [&](std::optional<Connection<>::ClientHash> _client_hash)
            {
                assert(_client_hash.has_value());
                ProcessWorkerDisconnection(_client_hash.value());
            });
    }


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

    ThreadLocalStorage::AcknowledgeFrameEnd();
}

bool Jani::Runtime::TryAllocateNewWorker(
    LayerId                  _layer_id,
    Connection<>::ClientHash _client_hash,
    WorkerType               _type)
{
    assert(m_worker_instance_mapping.find(_client_hash) == m_worker_instance_mapping.end());

    if (!m_layer_config.HasLayer(_layer_id))
    {
        return false;
    }

    auto layer_info = m_layer_config.GetLayerInfo(_layer_id);

    // Make sure the layer is an user layer if the requester is also an user
    if (layer_info.user_layer && _type != WorkerType::Client)
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
        auto new_bridge = std::make_unique<RuntimeBridge>(*this, _layer_id);
        m_bridges.insert({ _layer_id , std::move(new_bridge) });
        bridge_iter = m_bridges.find(_layer_id);
    }

    auto& bridge = bridge_iter->second;

    auto worker_instance = std::make_unique<RuntimeWorkerReference>(
        *bridge,
        _layer_id,
        _client_hash,
        _type);

    RuntimeWorkerReference* worker_instance_ptr = worker_instance.get();

    if (!m_world_controller->AddWorkerForLayer(std::move(worker_instance), _layer_id))
    {
        return false;
    }

    // Add an entry into our worker instance mapping to provide future mapping between
    // worker messages and the bridge callbacks
    m_worker_instance_mapping.insert({ _client_hash, worker_instance_ptr });

    return true;
}

bool Jani::Runtime::OnWorkerLogMessage(
    RuntimeWorkerReference& _worker_instance,
    WorkerId        _worker_id,
    WorkerLogLevel  _log_level,
    std::string     _log_title,
    std::string     _log_message)
{
    switch (_log_level)
    {
        case WorkerLogLevel::Trace: Jani::MessageLog().Trace("Worker {} -> {}: {}", _worker_id, _log_title, _log_message); break;
        case WorkerLogLevel::Info: Jani::MessageLog().Info("Worker {} -> {}: {}", _worker_id, _log_title, _log_message); break;
        case WorkerLogLevel::Warning: Jani::MessageLog().Warning("Worker {} -> {}: {}", _worker_id, _log_title, _log_message); break;
        case WorkerLogLevel::Error: Jani::MessageLog().Error("Worker {} -> {}: {}", _worker_id, _log_title, _log_message); break;
        case WorkerLogLevel::Critical: Jani::MessageLog().Critical("Worker {} -> {}: {}", _worker_id, _log_title, _log_message); break;
    }
    
    return true;
}

std::optional<Jani::EntityId> Jani::Runtime::OnWorkerReserveEntityIdRange(
    RuntimeWorkerReference& _worker_instance,
    WorkerId        _worker_id,
    uint32_t        _total_ids)
{
    return m_database.ReserveEntityIdRange(_total_ids);
}

bool Jani::Runtime::OnWorkerAddEntity(
    RuntimeWorkerReference& _worker_instance,
    WorkerId                _worker_id,
    EntityId                _entity_id,
    EntityPayload           _entity_payload)
{
    // Theoretically this below isn't necessary because now we can keep track of entities that
    // are layer-orphans, they will be assigned to a worker as soon as one gets available
    /*
    // Quick check if there are layers available for all requested components
    for (auto requested_component_payload : _entity_payload.component_payloads)
    {
        if (!IsLayerForComponentAvailable(requested_component_payload.component_id))
        {
            return false;
        }
    }
    */

    // Create the new entity on the database
    auto entity = m_database.AddEntity(_worker_id, _entity_id, _entity_payload); // Do not move since it will be used below
    if (!entity)
    {
        JaniWarning("Runtime -> OnWorkerAddEntity() failed for worker {} with entity id {}", _worker_id, _entity_id);
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
        LayerId layer_id = m_layer_config.GetLayerIdForComponent(requested_component_payload.component_id);
        auto worker      = entity.value()->GetWorldCellInfo().GetWorkerForLayer(layer_id);
        if (!worker)
        {
            // If there is no worker available, we just need to ignore this component addition
            // A worker will receive this component as soon as one gets available on the world
            // controller

            JaniTrace("Runtime -> OnWorkerAddEntity() Tried to call GetWorkerForLayer({}) on the current entity cell but no worker is available (worker requester {})", layer_id, _worker_id);
            continue;
        }

        Message::WorkerAddComponentRequest add_component_request;
        add_component_request.entity_id         = _entity_id;
        add_component_request.component_id      = requested_component_payload.component_id;
        add_component_request.component_payload = std::move(requested_component_payload); // Safe to move, this is the last usage

        // Send a message to this worker to acknowledge him about receiving the component/entity
        if (!m_request_manager->MakeRequest(
            *m_worker_connections,
            worker.value()->GetConnectionClientHash(), 
            RequestType::WorkerAddComponent,
            add_component_request))
        {
            JaniError("Runtime -> OnWorkerAddEntity() request to add a component to worker {} failed for entity {}, this could potentially cause future issues", _worker_id, _entity_id);

            continue;
        }
    }
    
    return true;
}

bool Jani::Runtime::OnWorkerRemoveEntity(
    RuntimeWorkerReference& _worker_instance,
    WorkerId        _worker_id,
    EntityId        _entity_id)
{
    if (!m_database.RemoveEntity(_worker_id, _entity_id))
    {
        JaniWarning("Runtime -> OnWorkerRemoveEntity() failed for worker {} with entity id {}", _worker_id, _entity_id);
        return false;
    }

    /*
    // Setup layer workers authority gain
    {
        for (int i = 0; i < MaximumLayers; i++)
        {
            LayerId layer_id = m_layer_config.GetLayerIdForComponent(i);
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
    RuntimeWorkerReference& _worker_instance,
    WorkerId                _worker_id,
    EntityId                _entity_id,
    ComponentId             _component_id,
    ComponentPayload        _component_payload)
{
    LayerId layer_id = m_layer_config.GetLayerIdForComponent(_component_id);

    // Theoretically this below isn't necessary because now we can keep track of entities that
    // are layer-orphans, they will be assigned to a worker as soon as one gets available
    /*
    // Quick check if there is a layer available to this component
    if (!IsLayerForComponentAvailable(_component_id))
    {
        return false;
    }
    */

    // Register this component on the database for the given entity
    auto entity = m_database.AddComponent(
        _worker_id, 
        _entity_id, 
        layer_id,
        _component_id, 
        _component_payload); // Do not move since it will be used below
    if (!entity)
    {
        JaniWarning("Runtime -> OnWorkerAddComponent() failed to add the component on the database for worker {} with entity id {}", _worker_id, _entity_id);
        return false;
    }

    {
        LayerId layer_id = m_layer_config.GetLayerIdForComponent(_component_id);
        auto worker      = entity.value()->GetWorldCellInfo().GetWorkerForLayer(layer_id);
        if (!worker)
        {
            // If there is no worker available, we don't need to worry about a worker receiving
            // the request to add the component since there is no worker available for this layer
            // A worker will receive this component as soon as one gets available on the world
            // controller

            JaniTrace("Runtime -> OnWorkerAddComponent() Tried to call GetWorkerForLayer({}) on the current entity cell but no worker is available (worker requester {})", layer_id, _worker_id);
        }
        else
        {
            Message::WorkerAddComponentRequest add_component_request;
            add_component_request.entity_id         = _entity_id;
            add_component_request.component_id      = _component_id;
            add_component_request.component_payload = std::move(_component_payload); // Safe to move, this is the last usage

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
    }

    return true;
}

bool Jani::Runtime::OnWorkerRemoveComponent(
    RuntimeWorkerReference& _worker_instance,
    WorkerId        _worker_id,
    EntityId        _entity_id,
    ComponentId     _component_id)
{
    LayerId layer_id = m_layer_config.GetLayerIdForComponent(_component_id);

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
        JaniWarning("Runtime -> OnWorkerRemoveComponent() failed to remove the component on the database for worker {} with entity id {}", _worker_id, _entity_id);
        return false;
    }

    return true;
}

bool Jani::Runtime::OnWorkerComponentUpdate(
    RuntimeWorkerReference&      _worker_instance,
    WorkerId                     _worker_id,
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    ComponentPayload             _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    LayerId layer_id = m_layer_config.GetLayerIdForComponent(_component_id);

    // TODO: Change this to accept a global variable that enabled deep validation
    if (true)
    {
        auto entity = m_database.GetEntityById(_entity_id);
        if (!entity)
        {
            JaniWarning("Runtime -> OnWorkerComponentUpdate() no entity exist with id {} (worker requester {})", _entity_id, _worker_id);
            return false;
        }

        auto& cell_info  = m_world_controller->GetWorldCellInfo(entity.value()->GetWorldCellCoordinates());
        auto cell_worker = cell_info.GetWorkerForLayer(layer_id);
        if (!cell_worker)
        {
            JaniTrace("Runtime -> OnWorkerComponentUpdate() no worker owner is available on layer {} for entity id {} (worker requester {})", layer_id, _entity_id, _worker_id);
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
        std::move(_component_payload),
        _entity_world_position);
    if (!entity)
    {
        JaniWarning("Runtime -> OnWorkerComponentUpdate() no worker owner is available on layer {} for entity id {} (worker requester {})", layer_id, _entity_id, _worker_id);
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
    RuntimeWorkerReference&     _worker_instance,
    WorkerId                    _worker_id,
    EntityId                    _entity_id,
    ComponentId                 _component_id,
    std::vector<ComponentQuery> _component_queries)
{
    auto entity = m_database.GetEntityByIdMutable(_entity_id);
    if (!entity)
    {
        JaniWarning("Runtime -> OnWorkerComponentInterestQueryUpdate() no entity exist with id {} (worker requester {})", _entity_id, _worker_id);
        return false;
    }

    LayerId layer_id    = m_layer_config.GetLayerIdForComponent(_component_id);
    auto&   cell_info   = m_world_controller->GetWorldCellInfo(entity.value()->GetWorldCellCoordinates());
    auto    cell_worker = cell_info.GetWorkerForLayer(layer_id);
    if (!cell_worker)
    {
        JaniTrace("Runtime -> OnWorkerComponentInterestQueryUpdate() no worker owner is available on layer {} for entity id {} (worker requester {})", layer_id, _entity_id, _worker_id);
        return false;
    }

    // This can happen if we just changed the owned of an entity layer and it didn't received the message yet or
    // it arrived before it sent an update
    if (_worker_id != cell_worker.value()->GetId())
    {
        return false;
    }

    // Use the highest frequency
    QueryUpdateFrequency frequency = QueryUpdateFrequency::Min;
    for (auto& query_info : _component_queries)
    {
        frequency = static_cast<QueryUpdateFrequency>(std::max(static_cast<uint32_t>(frequency), static_cast<uint32_t>(query_info.frequency)));
    }

    entity.value()->UpdateQueriesForComponent(_component_id, std::move(_component_queries));

    m_entity_query_controller.InsertQuery(_entity_id, _component_id, frequency, entity.value()->GetQueryVersion(_component_id));

    return true;
}

nonstd::transient_vector<std::pair<Jani::ComponentMask, nonstd::transient_vector<const Jani::ComponentPayload*>>> Jani::Runtime::PerformComponentQuery(
    const ComponentQuery&   _query, 
    WorldPosition           _search_center_location,
    std::optional<WorkerId> _ignore_worker) const
{
    nonstd::transient_vector<std::pair<ComponentMask, nonstd::transient_vector<const ComponentPayload*>>> query_result;
    nonstd::transient_unordered_map<EntityId, ServerEntity*>                                              selected_entities;

    if (!_query.IsValid())
    {
        return std::move(query_result);
    }

    bool has_area_query = false;
    std::function<void(const ComponentQueryInstruction&)> ProcessQueryInstruction = [&](const ComponentQueryInstruction& _current_query_instruction)
    {
        if (_current_query_instruction.box_constraint)
        {
            if (selected_entities.size() == 0 && !has_area_query)
            {
                has_area_query = true;
                m_world_controller->ForEachEntityOnRect(
                    _current_query_instruction.box_constraint.value(),
                    [&](EntityId _selected_entity_id, ServerEntity& _selected_entity, WorldCellCoordinates _cell_coordinates)
                    {
                        selected_entities.insert({ _selected_entity_id, &_selected_entity });
                    });
            }
            else
            {
                auto iter = selected_entities.begin();
                while (iter != selected_entities.end())
                {
                    auto& selected_entity      = iter->second;
                    glm::vec2 entity_position  = selected_entity->GetWorldPosition();
                    glm::vec2 min_box_position = glm::vec2(_current_query_instruction.box_constraint->x, _current_query_instruction.box_constraint->y);
                    glm::vec2 max_box_position = min_box_position + glm::vec2(_current_query_instruction.box_constraint->width, _current_query_instruction.box_constraint->height);
                    if (entity_position.x < min_box_position.x 
                        || entity_position.y < min_box_position.y
                        || entity_position.x > max_box_position.x
                        || entity_position.y > max_box_position.y)
                    {
                        iter = selected_entities.erase(iter);
                    }
                    else
                    {
                        iter++;
                    }
                }
            }
        }
        else if (_current_query_instruction.area_constraint)
        {
            if (selected_entities.size() == 0 && !has_area_query)
            {
                has_area_query          = true;
                WorldPosition half_area = WorldPosition({
                    static_cast<int32_t>(_current_query_instruction.area_constraint->width / 2),
                    static_cast<int32_t>(_current_query_instruction.area_constraint->height / 2) });
                WorldRect     rect      = WorldRect({
                    _search_center_location.x - half_area.x , _search_center_location.y - half_area.y,
                    _search_center_location.x + half_area.x , _search_center_location.y + half_area.y });
                m_world_controller->ForEachEntityOnRect(
                    rect,
                    [&](EntityId _selected_entity_id, ServerEntity& _selected_entity, WorldCellCoordinates _cell_coordinates)
                    {
                        selected_entities.insert({ _selected_entity_id, &_selected_entity });
                    });
            }
            else
            {
                auto iter = selected_entities.begin();
                while (iter != selected_entities.end())
                {
                    auto& selected_entity      = iter->second;
                    glm::vec2 half_area        = glm::vec2(_current_query_instruction.area_constraint->width, _current_query_instruction.area_constraint->height) / 2.0f;
                    glm::vec2 entity_position  = selected_entity->GetWorldPosition();
                    glm::vec2 min_box_position = glm::vec2(_search_center_location) - half_area;
                    glm::vec2 max_box_position = glm::vec2(_search_center_location) + half_area;
                    if (entity_position.x < min_box_position.x 
                        || entity_position.y < min_box_position.y
                        || entity_position.x > max_box_position.x
                        || entity_position.y > max_box_position.y)
                    {
                        iter = selected_entities.erase(iter);
                    }
                    else
                    {
                        iter++;
                    }
                }
            }
        }
        else if (_current_query_instruction.radius_constraint)
        {
            if (selected_entities.size() == 0 && !has_area_query)
            {
                has_area_query = true;
                m_world_controller->ForEachEntityOnRadius(
                    _search_center_location,
                    _current_query_instruction.radius_constraint.value(),
                    [&](EntityId _selected_entity_id, ServerEntity& _selected_entity, WorldCellCoordinates _cell_coordinates)
                    {
                        selected_entities.insert({ _selected_entity_id, &_selected_entity });
                    });
            }
            else
            {
                auto iter = selected_entities.begin();
                while (iter != selected_entities.end())
                {
                    auto& selected_entity     = iter->second;
                    glm::vec2 entity_position = selected_entity->GetWorldPosition();
                    glm::vec2 center_position = _search_center_location;
                    if (glm::distance(entity_position, center_position) > _current_query_instruction.radius_constraint.value())
                    {
                        iter = selected_entities.erase(iter);
                    }
                    else
                    {
                        iter++;
                    }
                }
            }
        }
        else if (_current_query_instruction.component_constraints)
        {
            if (selected_entities.size() == 0 && !has_area_query)
            {
                const auto& entities = m_database.GetEntities();
                for (auto& [entity_id, entity] : entities)
                {
                    auto total_required_components = _current_query_instruction.component_constraints.value().count();
                    auto component_mask            = entity->GetComponentMask();
                    if ((_current_query_instruction.component_constraints.value() & component_mask).count() == total_required_components)
                    {
                        selected_entities.insert({ entity_id, entity.get() });
                    }
                }
            }
            else
            {
                auto iter = selected_entities.begin();
                while (iter != selected_entities.end())
                {
                    auto& selected_entity          = iter->second;
                    auto total_required_components = _current_query_instruction.component_constraints.value().count();
                    auto component_mask            = selected_entity->GetComponentMask();
                    if ((_current_query_instruction.component_constraints.value() & component_mask).count() != total_required_components)
                    {
                        iter = selected_entities.erase(iter);
                    }
                    else
                    {
                        iter++;
                    }
                }
            }
        }
        else if (_current_query_instruction.and_constraint)
        {
            ProcessQueryInstruction(*_current_query_instruction.and_constraint->first);
            ProcessQueryInstruction(*_current_query_instruction.and_constraint->second);
        }
        else if (_current_query_instruction.or_constraint)
        {
            ProcessQueryInstruction(*_current_query_instruction.or_constraint->first);
            ProcessQueryInstruction(*_current_query_instruction.or_constraint->second);
        }
    };

    ProcessQueryInstruction(*_query.root_query);

    for (auto& [entity_id, entity] : selected_entities)
    {
        std::pair<ComponentMask, nonstd::transient_vector<const ComponentPayload*>> entry;
        entry.first = entity->GetComponentMask();

        for (const auto& requested_component_id : bitset::indices_on(_query.component_mask))
        {
            // Check if we should ignore this
            LayerId component_layer             = m_layer_config.GetLayerIdForComponent(requested_component_id);
            auto current_component_layer_worker = m_world_controller->GetWorldCellInfo(entity->GetWorldCellCoordinates()).GetWorkerForLayer(component_layer);
            if (_ignore_worker 
                && current_component_layer_worker 
                && current_component_layer_worker.value()->GetId() == _ignore_worker.value())
            {
                continue;
            }

            if (entity->HasComponent(requested_component_id))
            {
                entry.second.push_back(&entity->GetComponentPayload(requested_component_id));
            }
        }

        query_result.push_back(std::move(entry));
    }

    return std::move(query_result);
}

bool Jani::Runtime::IsLayerForComponentAvailable(ComponentId _component_id) const
{
    // Convert the component id to its operating layer id
    LayerId layer_id = m_layer_config.GetLayerIdForComponent(_component_id);

    for (auto& [bridge_layer_id, bridge] : m_bridges)
    {
        if (bridge_layer_id == layer_id)
        {
            return true;
        }
    }

    return false;
}