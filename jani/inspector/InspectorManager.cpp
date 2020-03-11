////////////////////////////////////////////////////////////////////////////////
// Filename: InspectorManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include "InspectorManager.h"
#include "Renderer.h"

Jani::Inspector::InspectorManager::InspectorManager()
{
}

Jani::Inspector::InspectorManager::~InspectorManager()
{
}

bool Jani::Inspector::InspectorManager::Initialize(std::string _runtime_ip, uint32_t _runtime_port)
{
    m_renderer           = std::make_unique<Renderer>();
    m_main_window        = std::make_unique<MainWindow>();
    m_runtime_connection = std::make_unique<Jani::Connection<>>(0, _runtime_port, _runtime_ip.c_str());
    m_request_manager    = std::make_unique<Jani::RequestManager>();

    m_runtime_connection->SetTimeoutTime(3000);

    if (!m_renderer->Initialize(1280, 720))
    {
        return false;
    }

    if (!m_main_window->Initialize())
    {
        return false;
    }

    return true;
}

void Jani::Inspector::InspectorManager::Update(uint32_t _time_elapsed_ms)
{
    auto time_now = std::chrono::steady_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_time).count() > 20)
    {
        m_runtime_connection->Update();

        m_runtime_connection->DidTimeout(
            [&](std::optional<Jani::Connection<>::ClientHash> _client_hash)
            {
                m_should_disconnect = true;
            });

        m_request_manager->Update(
            *m_runtime_connection,
            [&](auto _client_hash, const Jani::RequestInfo& _request_info, const Jani::ResponsePayload& _resonse_payload) -> void
            {
                switch (_request_info.type)
                {
                    case Jani::RequestType::RuntimeGetEntitiesInfo:
                    {
                        auto infos = _resonse_payload.GetResponse< Jani::Message::RuntimeGetEntitiesInfoResponse>();
                        for (auto& [entity_id, world_position, worker_id, component_mask] : infos.entities_infos)
                        {
                            EntityInfo entity_info;
                            entity_info.id                             = entity_id;
                            entity_info.world_position                 = world_position;
                            entity_info.worker_id                      = worker_id;
                            entity_info.component_mask                 = component_mask;
                            entity_info.last_update_received_timestamp = std::chrono::steady_clock::now();

                            m_entities_infos[entity_id] = std::move(entity_info);
                        }

                        break;
                    }
                    case Jani::RequestType::RuntimeGetCellsInfos:
                    {
                        auto infos = _resonse_payload.GetResponse< Jani::Message::RuntimeGetCellsInfosResponse>();
                        for (auto& [worker_id, layer_id, rect, coordinates, total_entities] : infos.cells_infos)
                        {
                            CellInfo cell_info;
                            cell_info.worker_id                      = worker_id;
                            cell_info.layer_id                       = layer_id;
                            cell_info.rect                           = rect;
                            cell_info.position                       = WorldPosition({ rect.x, rect.y });
                            cell_info.coordinates                    = coordinates;
                            cell_info.total_entities                 = total_entities;
                            cell_info.last_update_received_timestamp = std::chrono::steady_clock::now();

                            m_cell_infos[coordinates] = std::move(cell_info);
                        }

                        break;
                    }
                    case Jani::RequestType::RuntimeGetWorkersInfos:
                    {
                        auto infos = _resonse_payload.GetResponse< Jani::Message::RuntimeGetWorkersInfosResponse>();
                        for (auto& [worker_id, total_entities, layer_id, network_traffic_received, network_traffic_sent, total_allocations, allocations_size] : infos.workers_infos)
                        {
                            WorkerInfo worker_info;
                            worker_info.worker_id                              = worker_id;
                            worker_info.total_entities                         = total_entities;
                            worker_info.layer_id                               = layer_id;
                            worker_info.total_network_data_received_per_second = network_traffic_received;
                            worker_info.total_network_data_sent_per_second     = network_traffic_sent;
                            worker_info.total_memory_allocations_per_second    = total_allocations;
                            worker_info.total_memory_allocated_per_second      = allocations_size;
                            worker_info.last_update_received_timestamp         = std::chrono::steady_clock::now();

                            m_workers_infos[worker_id] = std::move(worker_info);
                        }

                        break;
                    }
                }
            });

        m_last_update_time = time_now;
    }
    
    auto time_from_last_request_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_runtime_request_time).count();
    if (time_from_last_request_ms > 100)
    {
        Jani::Message::RuntimeGetEntitiesInfoRequest get_entities_info_request;
        Jani::Message::RuntimeGetCellsInfosRequest   get_cells_infos_request;
        Jani::Message::RuntimeGetWorkersInfosRequest get_workers_infos_request;

        if (!m_request_manager->MakeRequest(
            *m_runtime_connection,
            Jani::RequestType::RuntimeGetEntitiesInfo,
            get_entities_info_request))
        {
            Jani::MessageLog().Error("Inspector -> Failed to request entities infos");
        }

        if (!m_request_manager->MakeRequest(
            *m_runtime_connection,
            Jani::RequestType::RuntimeGetCellsInfos,
            get_cells_infos_request))
        {
            Jani::MessageLog().Error("Inspector -> Failed to request cells infos");
        }

        if (!m_request_manager->MakeRequest(
            *m_runtime_connection,
            Jani::RequestType::RuntimeGetWorkersInfos,
            get_workers_infos_request))
        {
            Jani::MessageLog().Error("Inspector -> Failed to request workers infos");
        }

        m_last_runtime_request_time = time_now;
    }

    ////////////
    // RENDER //
    ////////////

    if (!m_renderer->BeginRenderFrame())
    {
        m_should_disconnect = true;
        return;
    }

    auto window_sizes = m_renderer->GetWindowSize();

    m_main_window->Draw(
        window_sizes.first, 
        window_sizes.second, 
        m_cell_infos, 
        m_entities_infos, 
        m_workers_infos);

    m_renderer->EndRenderFrame();
}

bool Jani::Inspector::InspectorManager::ShouldDisconnect() const
{
    return m_should_disconnect;
}