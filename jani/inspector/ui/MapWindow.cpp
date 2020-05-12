////////////////////////////////////////////////////////////////////////////////
// Filename: MapWindow.cpp
////////////////////////////////////////////////////////////////////////////////
#include "MapWindow.h"
#include "imgui.h"
#include "..\imgui_extension.h"

Jani::Inspector::MapWindow::MapWindow(InspectorManager& _inspector_manager) : BaseWindow(_inspector_manager, "Viewport")
{
}

Jani::Inspector::MapWindow::~MapWindow()
{
}

bool Jani::Inspector::MapWindow::Initialize()
{

    return true;
}

void Jani::Inspector::MapWindow::Update()
{
    // TODO: Update the bounding box to reflect the previous one
    // ...

    m_entity_datas.clear();
    m_entity_ids.clear();

    // Entity data
    {
        auto entity_data_enum_index = magic_enum::enum_index(WindowDataType::EntityData);
        if (m_input_connections[entity_data_enum_index.value()] != std::nullopt)
        {
            auto* entity_data_input_window = m_input_connections[entity_data_enum_index.value()].value().window;

            if (!entity_data_input_window->WasUpdated())
            {
                entity_data_input_window->Update();
            }

            auto entity_data_input_values = entity_data_input_window->GetOutputEntityData();
            if (entity_data_input_values)
            {
                m_entity_datas = std::move(entity_data_input_values.value());
            }
        }

        for (auto& [entity_id, entity_data] : m_entity_datas)
        {
            m_entity_ids.push_back(entity_id);
        }
    }

    // Visualization settings
    {
        m_visualization_settings = std::nullopt;

        auto visualization_settings_enum_index = magic_enum::enum_index(WindowDataType::VisualizationSettings);
        if (m_input_connections[visualization_settings_enum_index.value()] != std::nullopt)
        {
            auto* visualization_settings_input_window = m_input_connections[visualization_settings_enum_index.value()].value().window;

            if (!visualization_settings_input_window->WasUpdated())
            {
                visualization_settings_input_window->Update();
            }

            auto visualization_settings_input_value = visualization_settings_input_window->GetOutputVisualizationSettings();
            if (visualization_settings_input_value)
            {
                m_visualization_settings = std::move(visualization_settings_input_value.value());
            }
        }
    }
}

void Jani::Inspector::MapWindow::Draw(
    const CellsInfos&   _cell_infos,
    const WorkersInfos& _workers_infos)
{
    // Discover the world size
    float begin_x = 0;
    float begin_y = 0;
    float end_x   = 0;
    float end_y   = 0;

    {
        for (auto& [entity_id, entity_info] : m_entity_datas)
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

    m_map_bounding_box.x = -m_pure_scroll.x;
    m_map_bounding_box.y = -m_pure_scroll.y;
    m_map_bounding_box.width  = ImGui::GetWindowSize().x;
    m_map_bounding_box.height = ImGui::GetWindowSize().y;

    m_map_bounding_box.width /= m_zoom_level;
    m_map_bounding_box.height /= m_zoom_level;

    static float LargeGridSize = 32.0f;
    static float MinorGridSize = 8.0f;

    begin_x += m_scroll.x;
    begin_y += m_scroll.y;
    end_x += m_scroll.x;
    end_y += m_scroll.y;

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

        for (float x = -_size.x / 2.0f + minor_grid_size * 2.0f; x <= _size.x / 2.0f + minor_grid_size * 2.0f + 1.0f; x += minor_grid_size)
        {
            _draw_list->AddLine(ImVec2(x + _scroll.x * _zoom_level, _position.y), ImVec2(x + _scroll.x * _zoom_level, _size.y + _position.y), minor_grid_color);
        }

        for (float y = -_size.y / 2.0f + minor_grid_size * 2.0f; y <= _size.y / 2.0f + minor_grid_size * 2.0f + 1.0f; y += minor_grid_size)
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

    DrawBackgroundLines(
        ImGui::GetWindowDrawList(), 
        m_scroll, 
        ImVec2(begin_x, begin_y), 
        ImVec2(end_x - begin_x, end_y - begin_y), 
        m_zoom_level);

    for (auto& [entity_id, entity_info] : m_entity_datas)
    {
        float  entity_size                 = 5.0f * m_zoom_level;
        auto   entity_color                = GetColorForIndex(0/*entity_info.worker_id*/);
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

    if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
    {
        m_is_scrolling = true;
    }
    else if (ImGui::IsMouseReleased(0))
    {
        m_is_scrolling = false;
    }

    if (m_is_scrolling)
    {
        m_scroll      = m_scroll + ImGui::GetIO().MouseDelta / m_zoom_level;
        m_pure_scroll = m_pure_scroll + ImGui::GetIO().MouseDelta / m_zoom_level;
    }

    if (ImGui::GetIO().MouseWheel && ImGui::IsWindowHovered())
    {
        m_zoom_level += ImGui::GetIO().MouseWheel / 10.0f;
        m_zoom_level = std::max(2.0f, m_zoom_level);
        m_zoom_level = std::min(100.0f, m_zoom_level);
    }

    BaseWindow::Draw(_cell_infos, _workers_infos);
}

std::optional<Jani::Inspector::WindowInputConnection> Jani::Inspector::MapWindow::DrawOutputOptions(ImVec2 _button_size)
{
#define FinishGroupAndReturn(return_value) ImGui::EndGroup(); return return_value;

    ImGui::BeginGroup();
    ImGui::Text("# Entity Data");
    {
        ImGui::Button("All", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::EntityData, WindowConnectionType::All }));
        }

        ImGui::Button("Selection", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::EntityData, WindowConnectionType::Selection }));
        }

        ImGui::Button("Viewport", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::EntityData, WindowConnectionType::Viewport }));
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();

    ImGui::BeginGroup();
    ImGui::Text("# Entity Id");
    {
        ImGui::Button("All", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::EntityId, WindowConnectionType::All }));
        }

        ImGui::Button("Selection", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::EntityId, WindowConnectionType::Selection }));
        }

        ImGui::Button("Viewport", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::EntityId, WindowConnectionType::Viewport }));
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();

    ImGui::BeginGroup();
    ImGui::Text("# Constraint");
    {
        ImGui::Button("Viewport Rect", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::Constraint, WindowConnectionType::ViewportRect }));
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();

    ImGui::BeginGroup();
    ImGui::Text("# Position");
    {
        ImGui::Button("All", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::Position, WindowConnectionType::All }));
        }

        ImGui::Button("Selection", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::Position, WindowConnectionType::Selection }));
        }

        ImGui::Button("Viewport", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::Position, WindowConnectionType::Viewport }));
        }
    }
    ImGui::EndGroup();

    return std::nullopt;

#undef FinishGroupAndReturn
}

bool Jani::Inspector::MapWindow::CanAcceptInputConnection(const WindowInputConnection& _connection)
{
    switch (_connection.data_type)
    {
        case WindowDataType::EntityData:
        {
            return true;
        }
        case WindowDataType::Constraint:
        {
            return true;
        }
    }

    return false;
}

std::optional<std::unordered_map<Jani::EntityId, Jani::Inspector::EntityData>> Jani::Inspector::MapWindow::GetOutputEntityData() const
{
    return m_entity_datas;
}

std::optional<std::vector<Jani::EntityId>> Jani::Inspector::MapWindow::GetOutputEntityId() const
{
    return m_entity_ids;
}

std::optional<std::vector<std::shared_ptr<Jani::Inspector::Constraint>>> Jani::Inspector::MapWindow::GetOutputConstraint() const
{
    auto constraint_data_enum_index = magic_enum::enum_index(WindowDataType::Constraint);
    if (m_input_connections[constraint_data_enum_index.value()] != std::nullopt)
    {
        auto* constraint_input_window = m_input_connections[constraint_data_enum_index.value()].value().window;

        if (!constraint_input_window->WasUpdated())
        {
            constraint_input_window->Update();
        }

        auto constraint_input_values = constraint_input_window->GetOutputConstraint();

        if (constraint_input_values)
        {
            // TODO: Should add the map bounding box here?
            // ...

            return std::move(constraint_input_values.value());
        }
    }

    return std::nullopt;
}

std::optional<std::vector<Jani::WorldPosition>> Jani::Inspector::MapWindow::GetOutputPosition() const
{
    return std::nullopt;
}

void Jani::Inspector::MapWindow::OnPositionChange(ImVec2 _current_pos, ImVec2 _new_pos, ImVec2 _delta)
{
    m_scroll = m_scroll + _delta / m_zoom_level;
}

void Jani::Inspector::MapWindow::OnSizeChange(ImVec2 _current_size, ImVec2 _new_size, ImVec2 _delta)
{
    m_scroll = m_scroll - _delta / m_zoom_level;
}