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
    m_runtime_connection = std::make_unique<Jani::Connection<>>(0, _runtime_port, _runtime_ip.c_str());
    m_request_manager    = std::make_unique<Jani::RequestManager>();

    m_runtime_connection->SetTimeoutTime(3000);

    if (!m_renderer->Initialize(1280, 720))
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
                        for (auto& [entity_id, world_position, worker_id] : infos.entities_infos)
                        {
                            EntityInfo entity_info;
                            entity_info.id                             = entity_id;
                            entity_info.world_position                 = world_position;
                            entity_info.worker_id                      = worker_id;
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
                        // std::cout << "Inspector -> Received info about {" << m_cells_infos.cells_infos.size() << "} cells infos" << std::endl;

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

        if (!m_request_manager->MakeRequest(
            *m_runtime_connection,
            Jani::RequestType::RuntimeGetEntitiesInfo,
            get_entities_info_request))
        {
            std::cout << "Inspector -> Failed to request entities infos" << std::endl;
        }

        if (!m_request_manager->MakeRequest(
            *m_runtime_connection,
            Jani::RequestType::RuntimeGetCellsInfos,
            get_cells_infos_request))
        {
            std::cout << "Inspector -> Failed to request cells infos" << std::endl;
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

    // Discover the world size
    float begin_x = 0;
    float begin_y = 0;
    float end_x   = 0;
    float end_y   = 0;

    {
        for (auto& [entity_id, entity_info] : m_entities_infos)
        {
            begin_x = std::min(begin_x, static_cast<float>(entity_info.world_position.x));
            begin_y = std::min(begin_y, static_cast<float>(entity_info.world_position.y));
            end_x   = std::max(end_x, static_cast<float>(entity_info.world_position.x));
            end_y   = std::max(end_y, static_cast<float>(entity_info.world_position.y));
        }

        for (auto& [cell_coordinate, cell_info]: m_cell_infos)
        {
            begin_x = std::min(begin_x, static_cast<float>(cell_info.rect.x));
            begin_y = std::min(begin_y, static_cast<float>(cell_info.rect.y));
            end_x   = std::max(end_x, cell_info.rect.x + static_cast<float>(cell_info.rect.width));
            end_y   = std::max(end_y, cell_info.rect.y + static_cast<float>(cell_info.rect.height));
        }
    }

    static float LargeGridSize = 128.0f;
    static float MinorGridSize = 8;

    begin_x += m_scroll.x - MinorGridSize;
    begin_y += m_scroll.y - MinorGridSize;
    end_x += m_scroll.x + MinorGridSize;
    end_y += m_scroll.y + MinorGridSize;

    begin_x *= m_zoom_level;
    begin_y *= m_zoom_level;
    end_x *= m_zoom_level;
    end_y *= m_zoom_level;

    auto DrawBackgroundLines = [](ImDrawList* _draw_list, ImVec2 _scroll, ImVec2 _position, ImVec2 _size, float _zoom_level)
    {
        float line_padding            = -1.0f;
        float large_grid_size         = static_cast<float>(LargeGridSize) * _zoom_level;
        float minor_grid_size         = static_cast<float>(MinorGridSize) * _zoom_level;
        ImColor center_grid_color     = ImColor(0, 0, 0, std::max(static_cast<int>(255 * _zoom_level), 160));
        ImColor large_grid_color      = ImColor(22, 22, 22, std::max(static_cast<int>(255 * _zoom_level), 160));
        ImColor minor_grid_color      = ImColor(52, 52, 52, std::max(static_cast<int>(255 * _zoom_level * _zoom_level), 96));

        for (float x = -_size.x; x < _size.x; x += minor_grid_size)
        {
            _draw_list->AddLine(ImVec2(x + _scroll.x * _zoom_level, _position.y), ImVec2(x + _scroll.x * _zoom_level, _size.y + _position.y), minor_grid_color);
        }

        for (float y = -_size.y; y < _size.y; y += minor_grid_size)
        {
            _draw_list->AddLine(ImVec2(_position.x, y + _scroll.y * _zoom_level), ImVec2(_size.x + _position.x, y + _scroll.y * _zoom_level), minor_grid_color);
        }
    };

    auto GetColorForIndex = [](uint64_t _index) -> ImVec4
    {
        float step = 7.0f;
        float r = std::abs(std::sin(_index / step));
        float g = std::abs(std::cos(_index / step));
        float b = std::abs(std::sin(_index / step * 3.0));
        return ImVec4(r, g, b, 1.0f);
    };

    bool open = true;
    ImGui::SetNextWindowPos(ImVec2(begin_x, begin_y));
    ImGui::SetNextWindowSize(ImVec2(end_x - begin_x, end_y - begin_y));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
    if (ImGui::Begin("World", &open, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove))
    {
        std::string total_entities_text = "Entities: " + std::to_string(m_entities_infos.size());
        std::string total_workers_text =  "Cells:  " + std::to_string(m_cell_infos.size());
        ImGui::Text(total_entities_text.c_str());
        ImGui::Text(total_workers_text.c_str());

        DrawBackgroundLines(
            ImGui::GetWindowDrawList(), 
            m_scroll, 
            ImVec2(begin_x, begin_y), 
            ImVec2(end_x - begin_x, end_y - begin_y), 
            m_zoom_level);

        for (auto& [entity_id, entity_info] : m_entities_infos)
        {
            float  entity_size                 = 5.0f * m_zoom_level;
            auto   entity_color                = GetColorForIndex(entity_info.worker_id);
            ImVec2 entity_transformed_position = ImVec2(entity_info.world_position.x + m_scroll.x, entity_info.world_position.y + m_scroll.y);
            entity_transformed_position        = ImVec2(entity_transformed_position.x * m_zoom_level, entity_transformed_position.y * m_zoom_level);

            ImVec2 triangle_a = ImVec2(entity_transformed_position.x - entity_size / 3, entity_transformed_position.y + entity_size / 3);
            ImVec2 triangle_b = ImVec2(entity_transformed_position.x, entity_transformed_position.y - entity_size / 3);
            ImVec2 triangle_c = ImVec2(entity_transformed_position.x + entity_size / 3, entity_transformed_position.y + entity_size / 3);

            ImGui::GetWindowDrawList()->AddQuad(triangle_a, triangle_b, triangle_c, triangle_a, ImColor(entity_color), 2);
        }

        int cell_render_index = 0;
        for (auto& [cell_coordinate, cell_info] : m_cell_infos)
        {
            auto worker_color = GetColorForIndex(cell_info.worker_id);

            if (cell_info.rect.width == 0 || cell_info.rect.height == 0)
            {
                continue;
            }

            float  border                            = 0.2f;
            ImVec2 worker_transformed_position_begin = ImVec2(cell_info.rect.x + m_scroll.x + border, cell_info.rect.y + m_scroll.y + border);
            ImVec2 worker_transformed_position_end   = ImVec2(cell_info.rect.x + cell_info.rect.width + m_scroll.x - border, cell_info.rect.y + cell_info.rect.height + m_scroll.y - border);
            worker_transformed_position_begin        = ImVec2(worker_transformed_position_begin.x * m_zoom_level, worker_transformed_position_begin.y * m_zoom_level);
            worker_transformed_position_end          = ImVec2(worker_transformed_position_end.x * m_zoom_level, worker_transformed_position_end.y * m_zoom_level);
            ImVec2 center_position                   = ImVec2((worker_transformed_position_end.x + worker_transformed_position_begin.x) / 2.0f, (worker_transformed_position_end.y + worker_transformed_position_begin.y) / 2.0f);

            ImGui::SetCursorScreenPos(ImVec2(worker_transformed_position_begin.x, worker_transformed_position_end.y));
            ImGui::TextColored(ImVec4(worker_color.x, worker_color.y, worker_color.z, 0.4), (std::string("worker_id: ") + std::to_string(cell_info.worker_id)).c_str());

            ImGui::GetWindowDrawList()->AddRect(
                worker_transformed_position_begin,
                worker_transformed_position_end,
                ImColor(worker_color));

            ImGui::GetWindowDrawList()->AddCircle(
                center_position,
                0.3f * m_zoom_level,
                ImColor(worker_color));

            ImGui::SetCursorScreenPos(worker_transformed_position_begin);
            ImVec2 region_size = ImVec2(worker_transformed_position_end.x - worker_transformed_position_begin.x, worker_transformed_position_end.y - worker_transformed_position_begin.y);

            ImGui::InvisibleButton("worker_region_tooltip", region_size);
            if (ImGui::IsItemHovered())
            {
                std::string layer_id_text = "layer_id: " + std::to_string(cell_info.layer_id);
                std::string total_entities_text = "total_entities: " + std::to_string(cell_info.total_entities);
                std::string position_text = "position: {" + std::to_string(center_position.x) + ", " + std::to_string(center_position.y) + "}";
                ImGui::BeginTooltip();
                ImGui::PushID(cell_render_index);
                ImGui::TextColored(ImVec4(worker_color.x + 0.2, worker_color.y + 0.2, worker_color.z + 0.2, 0.4), layer_id_text.c_str());
                ImGui::TextColored(ImVec4(worker_color.x + 0.2, worker_color.y + 0.2, worker_color.z + 0.2, 0.4), total_entities_text.c_str());
                ImGui::TextColored(ImVec4(worker_color.x + 0.2, worker_color.y + 0.2, worker_color.z + 0.2, 0.4), position_text.c_str());
                ImGui::PopID();

                ImGui::EndTooltip();
            }
        }

        ImGui::End();
    }
    ImGui::PopStyleColor(2);

    if (ImGui::IsMouseDown(0))
    {
        m_scroll.x += ImGui::GetIO().MouseDelta.x / m_zoom_level;
        m_scroll.y += ImGui::GetIO().MouseDelta.y / m_zoom_level;
    }

    if (ImGui::GetIO().MouseWheel)
    {
        m_zoom_level += ImGui::GetIO().MouseWheel / 10.0f;
        m_zoom_level = std::max(2.0f, m_zoom_level);
        m_zoom_level = std::min(100.0f, m_zoom_level);
    }

    m_renderer->EndRenderFrame();
}

bool Jani::Inspector::InspectorManager::ShouldDisconnect() const
{
    return m_should_disconnect;
}