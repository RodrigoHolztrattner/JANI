////////////////////////////////////////////////////////////////////////////////
// Filename: QueryWindow.cpp
////////////////////////////////////////////////////////////////////////////////
#include "QueryWindow.h"
#include "..\InspectorManager.h"
#include "imgui.h"
#include "..\imgui_extension.h"
#include "nonstd/bitset_iter.h"

Jani::Inspector::QueryWindow::QueryWindow(InspectorManager& _inspector_manager) : BaseWindow(_inspector_manager, "Query Editor")
{
    m_inspector_manager.RegisterQueryWindow(*this);
}

Jani::Inspector::QueryWindow::~QueryWindow()
{
    m_inspector_manager.UnregisterQueryWindow(*this);
}

bool Jani::Inspector::QueryWindow::Initialize()
{

    return true;
}

void Jani::Inspector::QueryWindow::Update()
{
    auto time_now = std::chrono::steady_clock::now();

    // Determine if any entity data has expired
    auto iter = m_entity_datas.begin();
    while (iter != m_entity_datas.end())
    {
        auto& entity_data = iter->second;

        if (std::chrono::duration_cast<std::chrono::milliseconds>(time_now - entity_data.last_update_received_timestamp).count() > 1000)
        {
            iter = m_entity_datas.erase(iter);
        }
        else
        {
            iter++;
        }
    }

    BaseWindow::Update();
}

void Jani::Inspector::QueryWindow::Draw(
    const CellsInfos&   _cell_infos,
    const WorkersInfos& _workers_infos)
{
    float horizontal_increment_segment       = 10.0f;
    bool  should_open_add_popup              = false;
    bool  should_open_select_component_popup = false;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(10.0f, 10.0f));
    ImGui::SetWindowFontScale(0.7f);

    auto DrawAddConstraintButton = [&](std::shared_ptr<Constraint>* _for_node, int& _group_id)
    {
        auto initial_cursor_pos = ImGui::GetCursorPos();

        ImGui::SetCursorPos(ImVec2(initial_cursor_pos.x + horizontal_increment_segment, initial_cursor_pos.y));

        ImGui::PushID(_group_id++);
        if (ImGui::Button("Add Constraint"))
        {
            m_add_new_constraint_target = _for_node;
            should_open_add_popup       = true;    
        }
        ImGui::PopID();

        ImGui::SetCursorPos(ImVec2(initial_cursor_pos.x, ImGui::GetCursorPos().y));
    };

    std::function<void(std::shared_ptr<Constraint>&, float, int&, bool)> DrawConstraintNode = [&](
        std::shared_ptr<Constraint>& _constraint, 
        float                        _horizontal_padding,
        int&                         _group_id, 
        bool                         _is_locked = false) -> void
    {
        auto  initial_cursor_pos = ImGui::GetCursorPos();
        float internal_indent    = horizontal_increment_segment;
        bool  clear              = false;

        ImGui::SetCursorPos(ImVec2(initial_cursor_pos.x + _horizontal_padding, initial_cursor_pos.y));
        ImGui::BeginGroup();
        ImGui::PushID(_group_id++);
        if (_is_locked) { ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); }
        if (_constraint->box_constraint)
        {
            if (ImGui::Button("Box")) { clear = true; }
            ImGui::Indent(internal_indent);
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragInt2("###Position", &_constraint->box_constraint.value().x);
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragInt2("###Size", &_constraint->box_constraint.value().width, 1.0f, 0, 10000000);
            ImGui::Unindent(internal_indent);
        }
        else if (_constraint->area_constraint)
        {
            if (ImGui::Button("Area")) { clear = true; }
            ImGui::Indent(internal_indent);
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragInt2("###Size", reinterpret_cast<int32_t*>(&_constraint->area_constraint.value().width), 1.0f, 0, 10000000);
            ImGui::Unindent(internal_indent);
        }
        else if (_constraint->radius_constraint)
        {
            if (ImGui::Button("Radius")) { clear = true; }
            ImGui::Indent(internal_indent);
            ImGui::SetNextItemWidth(100.0f);
            ImGui::DragFloat("###Radius", &_constraint->radius_constraint.value(), 1.0f, 0.0f, 10000000.0f);
            ImGui::Unindent(internal_indent);
        }
        else if (_constraint->component_constraint)
        {
            if (ImGui::Button("Component")) { clear = true; }

            ImGui::SameLine();

            if (ImGui::Button("+"))
            {
                m_component_mask_selector_target = &_constraint->component_constraint.value();
                should_open_select_component_popup = true;
            }

            auto& layer_collection = m_inspector_manager.GetLayerCollection();
            auto& components_infos = layer_collection.GetComponents();

            ImGui::Indent(internal_indent);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.35f, 0.35f, 1.0f));
            for (int i = 0; i < _constraint->component_constraint.value().size(); i++)
            {
                auto component_info_iter = components_infos.find(i);
                if (component_info_iter != components_infos.end() && _constraint->component_constraint.value().test(i))
                {
                    auto& component_info = component_info_iter->second;
                    ImGui::SetNextItemWidth(100.0f);
                    if (ImGui::Button(component_info.name.c_str()))
                    {
                        _constraint->component_constraint.value().set(i, false);
                    }
                }
            }
            ImGui::PopStyleColor();
            ImGui::Unindent(internal_indent);
        }
        else if (_constraint->or_constraint)
        {
            if (ImGui::Button("Or")) { clear = true; }
            if (!_constraint->or_constraint.value().first)
            {
                DrawAddConstraintButton(&_constraint->or_constraint.value().first, _group_id);
            }
            else
            {
                DrawConstraintNode(_constraint->or_constraint.value().first, _horizontal_padding + internal_indent, _group_id, _is_locked);
            }

            if (!_constraint->or_constraint.value().second)
            {
                DrawAddConstraintButton(&_constraint->or_constraint.value().second, _group_id);
            }
            else
            {
                DrawConstraintNode(_constraint->or_constraint.value().second, _horizontal_padding + internal_indent, _group_id, _is_locked);
            }
        }
        else if (_constraint->and_constraint)
        {
            if (ImGui::Button("And")) { clear = true; }
            if (!_constraint->and_constraint.value().first)
            {
                DrawAddConstraintButton(&_constraint->and_constraint.value().first, _group_id);
            }
            else
            {
                DrawConstraintNode(_constraint->and_constraint.value().first, _horizontal_padding + internal_indent, _group_id, _is_locked);
            }

            if (!_constraint->and_constraint.value().second)
            {
                DrawAddConstraintButton(&_constraint->and_constraint.value().second, _group_id);
            }
            else
            {
                DrawConstraintNode(_constraint->and_constraint.value().second, _horizontal_padding + internal_indent, _group_id, _is_locked);
            }
        }
        if (_is_locked) { ImGui::PopItemFlag(); ImGui::PopStyleVar(); }

        if (clear)
        {
            m_add_new_constraint_target      = std::nullopt;
            m_component_mask_selector_target = std::nullopt;
            _constraint                      = nullptr;
        }

        ImGui::PopID();
        ImGui::EndGroup();
        ImGui::SetCursorPos(ImVec2(initial_cursor_pos.x, ImGui::GetCursorPos().y));
    };

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);

    int group_id = 0;
    {
        auto output_constraints = GetOutputConstraint();
        if (output_constraints)
        {
            for (auto& output_constraint : output_constraints.value())
            {
                DrawConstraintNode(output_constraint, horizontal_increment_segment, group_id, true);
            }

            if (output_constraints.value().size() > 0)
            {
                ImGui::Separator();
            }
        }  
    }

    if (!m_constraints)
    {
        DrawAddConstraintButton(&m_constraints, group_id);
    }
    else
    {
        DrawConstraintNode(m_constraints, horizontal_increment_segment, group_id, false);
    }

    if (should_open_add_popup)
    {
        ImGui::OpenPopup("Add Constraint Popup");
    }

    if (ImGui::BeginPopup("Add Constraint Popup") && m_add_new_constraint_target)
    {
        float button_width       = 150.0f;
        auto  initial_cursor_pos = ImGui::GetCursorPos();

        ImGui::SetCursorPos(ImVec2(initial_cursor_pos.x + horizontal_increment_segment, initial_cursor_pos.y));
        ImGui::BeginGroup();

        ImGui::SetWindowFontScale(0.7f);

        if (ImGui::Button("Box", ImVec2(button_width, 0.0f)))
        {
            if (m_add_new_constraint_target)
            {
                *m_add_new_constraint_target.value() = std::make_shared<Constraint>();
                (*m_add_new_constraint_target.value())->box_constraint = WorldRect();
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Radius", ImVec2(button_width, 0.0f)))
        {
            if (m_add_new_constraint_target)
            {
                *m_add_new_constraint_target.value() = std::make_shared<Constraint>();
                (*m_add_new_constraint_target.value())->radius_constraint = 0.0f;
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Area", ImVec2(button_width, 0.0f)))
        {
            if (m_add_new_constraint_target)
            {
                *m_add_new_constraint_target.value() = std::make_shared<Constraint>();
                (*m_add_new_constraint_target.value())->area_constraint = WorldArea();
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Component", ImVec2(button_width, 0.0f)))
        {
            if (m_add_new_constraint_target)
            {
                *m_add_new_constraint_target.value() = std::make_shared<Constraint>();
                (*m_add_new_constraint_target.value())->component_constraint = ComponentMask();
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Or", ImVec2(button_width, 0.0f)))
        {
            if (m_add_new_constraint_target)
            {
                *m_add_new_constraint_target.value() = std::make_shared<Constraint>();
                (*m_add_new_constraint_target.value())->or_constraint = std::pair<std::shared_ptr<Constraint>, std::shared_ptr<Constraint>>();
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("And", ImVec2(button_width, 0.0f)))
        {
            if (m_add_new_constraint_target)
            {
                *m_add_new_constraint_target.value() = std::make_shared<Constraint>();
                (*m_add_new_constraint_target.value())->and_constraint = std::pair<std::shared_ptr<Constraint>, std::shared_ptr<Constraint>>();
            }

            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Not", ImVec2(button_width, 0.0f)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndGroup();
        ImGui::SetCursorPos(ImVec2(initial_cursor_pos.x, ImGui::GetCursorPos().y));

        ImGui::SetWindowFontScale(1.0f);

        ImGui::EndPopup();
    }
    else
    {
        m_add_new_constraint_target = std::nullopt;
    }

    if (should_open_select_component_popup)
    {
        ImGui::OpenPopup("Component Mask Selector Popup");
    }
    
    if (ImGui::BeginPopup("Component Mask Selector Popup") && m_component_mask_selector_target)
    {
        ComponentMask& component_mask = *m_component_mask_selector_target.value();

        auto& layer_collection = m_inspector_manager.GetLayerCollection();
        auto& components_infos = layer_collection.GetComponents();

        for (int i = 0; i < component_mask.size(); i++)
        {
            float button_width       = 150.0f;
            auto component_info_iter = components_infos.find(i);
            if (component_info_iter != components_infos.end())
            {
                auto& component_info = component_info_iter->second;
                
                bool is_selected = component_mask.test(i);
                ImGui::SetNextItemWidth(button_width);
                if (ImGui::Checkbox(component_info.name.c_str(), &is_selected))
                {

                }

                component_mask.set(i, is_selected);
            }
        }

        ImGui::EndPopup();
    }
    else
    {
        m_component_mask_selector_target = std::nullopt;
    }

    {
        ImGui::SetWindowFontScale(0.8f);

        float bottom_height    = std::max(180.0f, 80.0f + m_constraint_required_components.count() * 30.0f);
        float remaining_height = ImGui::GetWindowSize().y - ImGui::GetCursorPos().y;
        float height_diff      = remaining_height - bottom_height;

        ImVec2 bottom_start_pos = ImVec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y + std::max(0.0f, height_diff));
        ImVec2 bottom_end_pos   = bottom_start_pos + ImVec2(ImGui::GetWindowSize().x, remaining_height);

        ImGui::GetWindowDrawList()->AddRectFilled(
            bottom_start_pos,
            bottom_end_pos,
            ImColor(ImVec4(0.2f, 0.2f, 0.2f, 1.0f)), 
            4.0f, 
            ImDrawCornerFlags_Top);

        ImGui::SetCursorScreenPos(bottom_start_pos + ImVec2(10.0f, 10.0f));

        ImGui::BeginGroup();

        std::string components_stream_text = "Components Streamed with Query (" + std::to_string(m_constraint_required_components.count()) + ")";
        ImGui::Text(components_stream_text.c_str());
        
        ImGui::SameLine();
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowSize().x - 40.0f, ImGui::GetCursorPos().y));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        if (ImGui::Button("+"))
        {
            m_component_mask_selector_target = &m_constraint_required_components;
            ImGui::OpenPopup("Component Mask Selector Popup");
        }
        ImGui::PopStyleColor();

        auto& layer_collection = m_inspector_manager.GetLayerCollection();
        auto& components_infos = layer_collection.GetComponents();

        for (int i = 0; i < m_constraint_required_components.size(); i++)
        {
            auto component_info_iter = components_infos.find(i);
            if (component_info_iter != components_infos.end() && m_constraint_required_components.test(i))
            {
                auto& component_info = component_info_iter->second;
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::Button(component_info.name.c_str()))
                {
                    m_constraint_required_components.set(i, false);
                }
            }
        }

        float execute_button_padding = 5.0f;
        float execute_button_height  = 25.0f;
        ImGui::SetCursorScreenPos(ImVec2(bottom_start_pos.x + execute_button_padding, bottom_start_pos.y + bottom_height - (execute_button_height + execute_button_padding)));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(m_query_active ? 0.7f : 0.4f, m_query_active ? 0.4f : 0.7f, 0.4f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(m_query_active ? 0.8f : 0.6f, m_query_active ? 0.6f : 0.8f, 0.6f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(m_query_active ? 0.7f : 0.5f, m_query_active ? 0.5f : 0.7f, 0.5f, 1.0f));

        float execute_button_width = ImGui::GetWindowSize().x - execute_button_padding * 2.0f;

        if (m_query_active)
        {
            ImGui::SetWindowFontScale(1.0f);
            ImGui::Text((std::string("Querying ") + std::to_string(m_entity_datas.size()) + " entities").c_str());
            ImGui::SetWindowFontScale(0.8f);
            ImGui::SameLine();

            execute_button_width = 60.0f;
            ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + (ImGui::GetWindowSize().x - ImGui::GetCursorPos().x) - execute_button_width - execute_button_padding, ImGui::GetCursorPos().y));
        }

        if (ImGui::Button(m_query_active ? "Stop" : "Run", ImVec2(execute_button_width, execute_button_height)))
        {
            m_query_active = !m_query_active;
        }
        ImGui::PopStyleColor(3);

        ImGui::EndGroup();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar();

    BaseWindow::Draw(_cell_infos, _workers_infos);
}

std::optional<Jani::Inspector::WindowInputConnection> Jani::Inspector::QueryWindow::DrawOutputOptions(ImVec2 _button_size)
{
#define FinishGroupAndReturn(return_value) ImGui::EndGroup(); return return_value;

    ImGui::BeginGroup();
    ImGui::Text("# Entity Data");
    {
        ImGui::Button("Entity Data", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::EntityData, WindowConnectionType::All }));
        }
    }
    ImGui::EndGroup();

    ImGui::Separator();

    ImGui::BeginGroup();
    ImGui::Text("# Constraint");
    {
        ImGui::Button("Constraint", _button_size);
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0))
        {
            FinishGroupAndReturn(WindowInputConnection({ this, WindowDataType::Constraint, WindowConnectionType::Constraint }));
        }
    }
    ImGui::EndGroup();

    return std::nullopt;

#undef FinishGroupAndReturn
}

bool Jani::Inspector::QueryWindow::CanAcceptInputConnection(const WindowInputConnection& _connection)
{
    switch (_connection.data_type)
    {
        case WindowDataType::EntityData:
        {
            return true;
        }
        case WindowDataType::Constraint:
        {
            return _connection.connection_type == WindowConnectionType::ViewportRect;
        }
    }

    return false;
}

void Jani::Inspector::QueryWindow::ReceiveEntityQueryResults(const ComponentQueryResultPayload& _query_payload)
{
    auto entity_iter = m_entity_datas.find(_query_payload.entity_id);
    if (entity_iter != m_entity_datas.end())
    {
        auto& stored_entity_data                          = entity_iter->second;
        stored_entity_data.component_mask                 = _query_payload.entity_component_mask;
        stored_entity_data.world_position                 = _query_payload.entity_world_position;
        stored_entity_data.last_update_received_timestamp = std::chrono::steady_clock::now();

        for (auto& component_payload : _query_payload.component_payloads)
        {
            stored_entity_data.component_payloads[component_payload.component_id] = component_payload;
        }

        for (const auto& component_id : bitset::indices_off(_query_payload.entity_component_mask))
        {
            stored_entity_data.component_payloads[component_id] = std::nullopt;
        }
    }
    else
    {
        EntityData new_entity_data;
        new_entity_data.entity_id      = _query_payload.entity_id;
        new_entity_data.component_mask = _query_payload.entity_component_mask;
        new_entity_data.world_position = _query_payload.entity_world_position;

        for (auto& component_payload : _query_payload.component_payloads)
        {
            new_entity_data.component_payloads[component_payload.component_id] = component_payload;
        }

        m_entity_datas.insert({ _query_payload.entity_id, std::move(new_entity_data) });
    }
}

std::optional<std::pair<Jani::WorldPosition, Jani::ComponentQuery>> Jani::Inspector::QueryWindow::GetComponentQueryPayload() const
{
    if (!m_query_active)
    {
        return std::nullopt;
    }

    auto constraints = GetOutputConstraint();
    if ((!constraints || constraints.value().size() == 0 || m_constraint_required_components.count() == 0) && !m_constraints)
    {
        return std::nullopt;
    }

    ComponentQuery component_query;
    auto* query_instruction = component_query
        .QueryComponents(m_constraint_required_components)
        .Begin();

    std::function<void(const Constraint&, ComponentQueryInstruction&)> ApplyConstraintToQuery = [&](const Constraint& _constraint, ComponentQueryInstruction& _query)
    {
        if (_constraint.box_constraint)
        {
            _query.EntitiesInRect(_constraint.box_constraint.value());
        }
        else if (_constraint.area_constraint)
        {
            _query.EntitiesInArea(_constraint.area_constraint.value());
        }
        else if (_constraint.radius_constraint)
        {
            _query.EntitiesInRadius(_constraint.radius_constraint.value());
        }
        else if (_constraint.component_constraint)
        {
            _query.RequireComponents(_constraint.component_constraint.value());
        }
        else if (_constraint.or_constraint && _constraint.or_constraint.value().first && _constraint.or_constraint.value().second)
        {
            auto or_query = _query.Or();
            ApplyConstraintToQuery(*_constraint.and_constraint.value().first, *or_query.first);
            ApplyConstraintToQuery(*_constraint.and_constraint.value().second, *or_query.second);
        }
        else if (_constraint.and_constraint && _constraint.and_constraint.value().first && _constraint.and_constraint.value().second)
        {
            auto and_query = _query.And();
            ApplyConstraintToQuery(*_constraint.and_constraint.value().first, *and_query.first);
            ApplyConstraintToQuery(*_constraint.and_constraint.value().second, *and_query.second);
        }
    };

    for (int i = 0; i < constraints.value().size(); i++)
    {
        auto& constraint  = constraints.value()[i];
        auto and_query    = query_instruction->And();
        query_instruction = and_query.second;

        ApplyConstraintToQuery(*constraint, *and_query.first);
    }

    if (m_constraints)
    {
        ApplyConstraintToQuery(*m_constraints, *query_instruction);
    }

    if (!component_query.IsValid())
    {
        return std::nullopt;
    }

    return std::make_pair(WorldPosition({ 0, 0 }), std::move(component_query));
}

Jani::Inspector::WindowDataType Jani::Inspector::QueryWindow::GetInputTypes() const
{
    return WindowDataType::None;
}

Jani::Inspector::WindowDataType Jani::Inspector::QueryWindow::GetOutputTypes() const
{
    return static_cast<WindowDataType>(static_cast<int>(WindowDataType::EntityData) | static_cast<int>(WindowDataType::Constraint));
}

std::optional<std::unordered_map<Jani::EntityId, Jani::Inspector::EntityData>> Jani::Inspector::QueryWindow::GetOutputEntityData() const
{
    return m_entity_datas;
}

std::optional<std::vector<std::shared_ptr<Jani::Inspector::Constraint>>> Jani::Inspector::QueryWindow::GetOutputConstraint() const
{
    std::vector<std::shared_ptr<Constraint>> input_constraints;

    {
        // WindowDataType::Constraint
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
                    input_constraints.insert(input_constraints.end(), constraint_input_values.value().begin(), constraint_input_values.value().end());
                }
            }
        }

        // WindowDataType::Constraint
        {
            auto constraint_data_enum_index = magic_enum::enum_index(WindowDataType::Constraint);
            if (m_input_connections[constraint_data_enum_index.value()] != std::nullopt 
                && m_input_connections[constraint_data_enum_index.value()]->connection_type == WindowConnectionType::ViewportRect)
            {
                auto* constraint_input_window = m_input_connections[constraint_data_enum_index.value()].value().window;

                if (!constraint_input_window->WasUpdated())
                {
                    constraint_input_window->Update();
                }

                auto output_rect = constraint_input_window->GetOutputRect();

                if (output_rect)
                {
                    std::shared_ptr<Constraint> rect_constraint = std::make_shared<Constraint>();
                    rect_constraint->box_constraint = output_rect.value();
                    input_constraints.push_back(std::move(rect_constraint));
                }
            }
        }
    }
 
    return std::move(input_constraints);
}