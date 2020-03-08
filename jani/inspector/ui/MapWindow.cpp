////////////////////////////////////////////////////////////////////////////////
// Filename: MapWindow.cpp
////////////////////////////////////////////////////////////////////////////////
#include "MapWindow.h"
#include "imgui.h"
#include "..\imgui_extension.h"

Jani::Inspector::MapWindow::MapWindow()
{
}

Jani::Inspector::MapWindow::~MapWindow()
{
}

bool Jani::Inspector::MapWindow::Initialize()
{

    return true;
}

void Jani::Inspector::MapWindow::Draw(
    uint32_t             _window_width,
    const CellsInfos&    _cell_infos,
    const EntitiesInfos& _entities_infos)
{
    // Discover the world size
    float begin_x = 0;
    float begin_y = 0;
    float end_x   = 0;
    float end_y   = 0;

    {
        for (auto& [entity_id, entity_info] : _entities_infos)
        {
            begin_x = std::min(begin_x, static_cast<float>(entity_info.world_position.x));
            begin_y = std::min(begin_y, static_cast<float>(entity_info.world_position.y));
            end_x   = std::max(end_x, static_cast<float>(entity_info.world_position.x));
            end_y   = std::max(end_y, static_cast<float>(entity_info.world_position.y));
        }

        for (auto& [cell_coordinate, cell_info]: _cell_infos)
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

    if (ImGui::BeginChild("World", ImVec2(_window_width, 0), false, ImGuiWindowFlags_NoScrollbar))
    {
        std::string total_entities_text = "Entities: " + std::to_string(_entities_infos.size());
        std::string total_workers_text =  "Cells:  " + std::to_string(_cell_infos.size());
        ImGui::Text(total_entities_text.c_str());
        ImGui::Text(total_workers_text.c_str());

        DrawBackgroundLines(
            ImGui::GetWindowDrawList(), 
            m_scroll, 
            ImVec2(begin_x, begin_y), 
            ImVec2(end_x - begin_x, end_y - begin_y), 
            m_zoom_level);

        for (auto& [entity_id, entity_info] : _entities_infos)
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
        for (auto& [cell_coordinate, cell_info] : _cell_infos)
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

        if (ImGui::IsMouseDown(0) && ImGui::IsMouseHoveringWindow())
        {
            m_scroll.x += ImGui::GetIO().MouseDelta.x / m_zoom_level;
            m_scroll.y += ImGui::GetIO().MouseDelta.y / m_zoom_level;
        }

        if (ImGui::GetIO().MouseWheel && ImGui::IsMouseHoveringWindow())
        {
            m_zoom_level += ImGui::GetIO().MouseWheel / 10.0f;
            m_zoom_level = std::max(2.0f, m_zoom_level);
            m_zoom_level = std::min(100.0f, m_zoom_level);
        }

    } ImGui::EndChild();
}