////////////////////////////////////////////////////////////////////////////////
// Filename: BaseWindow.cpp
////////////////////////////////////////////////////////////////////////////////
#include "BaseWindow.h"
#include "imgui.h"
#include "..\imgui_extension.h"

Jani::Inspector::BaseWindow::BaseWindow(InspectorManager& _inspector_manager, const std::string _window_type_name) : m_inspector_manager(_inspector_manager)
{
    static WindowId unique_index_counter = 0;
    m_unique_id   = unique_index_counter++;
    m_name        = _window_type_name;
    m_unique_name = _window_type_name + "###" + std::to_string(m_unique_id);
}

Jani::Inspector::BaseWindow::~BaseWindow()
{
    RemoveAllInputWindowConnections(*this);
    RemoveAllOutputWindowConnections(*this);
}

void Jani::Inspector::BaseWindow::PreUpdate()
{
    m_updated = false;
}

void Jani::Inspector::BaseWindow::Update()
{
    m_updated = true;
}

void Jani::Inspector::BaseWindow::Draw(
    const CellsInfos&   _cell_infos,
    const WorkersInfos& _workers_infos)
{
}

void Jani::Inspector::BaseWindow::DrawConnections()
{
    auto DrawConnection = [](ImVec2 _from, ImVec2 _to, ImColor _color) -> void
    {
        ImVec2 lenght = ImVec2(_to.x - _from.x, _to.y - _from.y);
        float factor_from_points = std::sqrt(std::pow(lenght.x, 2) + std::pow(lenght.y, 2)) / 2.0f;

        auto pos0 = _from;
        auto cp0 = ImVec2(_from.x - factor_from_points, _from.y);
        auto cp1 = ImVec2(_to.x + factor_from_points, _to.y);
        auto pos1 = _to;

        ImGui::GetForegroundDrawList()->AddBezierCurve(
            pos0,
            cp0,
            cp1,
            pos1,
            _color,
            2);
    };

    auto GetColorForDataType = [](WindowDataType _data_type) -> ImColor
    {
        switch (_data_type)
        {
            case WindowDataType::EntityData:
            {
                return ImColor(ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
            }
            case WindowDataType::EntityId:
            {
                return ImColor(ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
            }
            case WindowDataType::Constraint:
            {
                return ImColor(ImVec4(0.3f, 0.3f, 0.9f, 1.0f));
            }
            case WindowDataType::Position:
            {
                return ImColor(ImVec4(0.7f, 0.9f, 0.3f, 1.0f));
            }
            case WindowDataType::VisualizationSettings:
            {
                return ImColor(ImVec4(0.3f, 0.7f, 0.9f, 1.0f));
            }
            default:
            {
                return ImColor(ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
            }
        }
    };

    for (int i = 0; i < m_output_connections.size(); i++)
    {
        auto& output_connection_info = m_output_connections[i];
        for (auto& [window_id, window] : output_connection_info)
        {
            DrawConnection(m_outputs_position, window->GetConnectionInputPosition(), GetColorForDataType(magic_enum::enum_value<WindowDataType>(i)));
        }
    }

    if (m_receiving_connection)
    {
        DrawConnection(m_receiving_connection->window->GetConnectionOutputPosition(), m_inputs_position, GetColorForDataType(m_receiving_connection->data_type));
    }

    if (m_creating_connection_payload)
    {
        DrawConnection(m_outputs_position, ImGui::GetMousePos(), GetColorForDataType(m_creating_connection_payload->data_type));
    }
}

bool Jani::Inspector::BaseWindow::CanAcceptInputConnection(const WindowInputConnection& _connection)
{
    return false;
}

std::optional<Jani::Inspector::WindowInputConnection> Jani::Inspector::BaseWindow::DrawOutputOptions(ImVec2 _button_size)
{
    return std::nullopt;
}

Jani::Inspector::WindowDataType Jani::Inspector::BaseWindow::GetInputTypes() const
{
    return WindowDataType::None;
}

Jani::Inspector::WindowDataType Jani::Inspector::BaseWindow::GetOutputTypes() const
{
    return WindowDataType::None;
}

std::optional<std::unordered_map<Jani::EntityId, Jani::Inspector::EntityData>> Jani::Inspector::BaseWindow::GetOutputEntityData() const
{
    return std::nullopt;
}

std::optional<std::vector<Jani::EntityId>> Jani::Inspector::BaseWindow::GetOutputEntityId() const
{
    return std::nullopt;
}

std::optional<std::vector<std::shared_ptr<Jani::Inspector::Constraint>>> Jani::Inspector::BaseWindow::GetOutputConstraint() const
{
    return std::nullopt;
}

std::optional<std::vector<Jani::WorldPosition>> Jani::Inspector::BaseWindow::GetOutputPosition() const
{
    return std::nullopt;
}

std::optional<Jani::Inspector::VisualizationSettings> Jani::Inspector::BaseWindow::GetOutputVisualizationSettings() const
{
    return std::nullopt;
}

void Jani::Inspector::BaseWindow::OnPositionChange(ImVec2 _current_pos, ImVec2 _new_pos, ImVec2 _delta)
{
    /* stub */
}

void Jani::Inspector::BaseWindow::OnSizeChange(ImVec2 _current_size, ImVec2 _new_size, ImVec2 _delta)
{
    /* stub */
}


Jani::Inspector::WindowId Jani::Inspector::BaseWindow::GetId() const
{
    return m_unique_id;
}

bool Jani::Inspector::BaseWindow::WasUpdated() const
{
    return m_updated;
}

ImVec2 Jani::Inspector::BaseWindow::GetWindowPos() const
{
    return m_window_pos;
}

ImVec2 Jani::Inspector::BaseWindow::GetWindowSize() const
{
    return m_is_visible ? m_window_size : ImVec2(m_window_size.x, m_header_height);
}

const std::string Jani::Inspector::BaseWindow::GetUniqueName() const
{
    return m_unique_name;
}

bool Jani::Inspector::BaseWindow::IsVisible() const
{
    return m_is_visible;
}

void Jani::Inspector::BaseWindow::DrawTitleBar(bool _edit_mode, std::optional<WindowInputConnection> _connection)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::BeginChild("Title Bar", ImVec2(ImGui::GetWindowSize().x, m_header_height), false, ImGuiWindowFlags_NoDecoration))
    {
        if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringWindow())
        {
            m_is_moving = true;
        }

        ImGui::AlignTextToFramePadding();
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + 5.0f, ImGui::GetCursorPos().y));
        if (ImGui::Button("#"))
        {
            m_is_visible = !m_is_visible;
        }
        ImGui::SameLine();
        ImGui::Text(m_name.c_str());

        ImGui::SameLine();

        ImVec2 current_cursor_pos = ImGui::GetCursorPos();
        float remaining_width     = ImGui::GetWindowSize().x;
        float options_button_size = ImGui::CalcTextSize("*").x + 20.0f;
        float input_button_size   = ImGui::CalcTextSize("+ INPUT").x + 5.0f;
        float output_button_size  = ImGui::CalcTextSize("- OUTPUT").x + 10.0f;

        float input_button_begin   = remaining_width - (options_button_size + input_button_size + output_button_size);
        float output_button_begin  = remaining_width - (options_button_size + output_button_size);
        float options_button_begin = remaining_width - options_button_size;

        if (_edit_mode)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 2.0f);
            ImGui::SetWindowFontScale(0.7f);
            ImGui::SetCursorPos(ImVec2(input_button_begin, current_cursor_pos.y + 3.0f));
        
            m_inputs_position = ImGui::GetCursorScreenPos();
            if (_connection && (_connection->window == this || !CanAcceptInputConnection(_connection.value()))) { ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); }
            ImGui::Button("+ INPUT");
            if (_connection && (_connection->window == this || !CanAcceptInputConnection(_connection.value()))) { ImGui::PopItemFlag(); ImGui::PopStyleVar(); }
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(0) && _connection && _connection->window != this && !ImGui::GetIO().KeyCtrl)
            {
                m_receiving_connection = _connection;
                ImGui::OpenPopup("Input Popup");
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0) && ImGui::GetIO().KeyCtrl)
            {
                RemoveAllInputWindowConnections(*this);
            }
            m_inputs_position = m_inputs_position + ImVec2(ImGui::GetItemRectSize().x / 2.0f, ImGui::GetItemRectSize().y / 2.0f);
        
            ImGui::SameLine();
            ImGui::SetCursorPos(ImVec2(output_button_begin, current_cursor_pos.y + 3.0f));
            m_outputs_position = ImGui::GetCursorScreenPos();
            if (_connection) { ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true); ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f); }
            if (ImGui::Button("- OUTPUT"))
            {
                ImGui::OpenPopup("Output Popup");
            }
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0) && ImGui::GetIO().KeyCtrl)
            {
                RemoveAllOutputWindowConnections(*this);
            }
            if (_connection) { ImGui::PopItemFlag(); ImGui::PopStyleVar(); }
            m_outputs_position = m_outputs_position + ImVec2(ImGui::GetItemRectSize().x / 2.0f, ImGui::GetItemRectSize().y / 2.0f);
            
            ImGui::SetWindowFontScale(1.0f);
            ImGui::PopStyleVar();
            ImGui::PopStyleColor(3);

            ImGui::SameLine();
        }

        ImGui::SetCursorPos(ImVec2(options_button_begin, current_cursor_pos.y));
        if (ImGui::Button("*"))
        {

        }

        ImGui::NewLine();

        if (ImGui::BeginPopup("Input Popup") && m_receiving_connection)
        {
            if (CanAcceptInputConnection(m_receiving_connection.value()))
            {
                AddWindowConnection(*m_receiving_connection->window, *this, m_receiving_connection->data_type, m_receiving_connection->connection_type);

                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else
        {
            m_receiving_connection = std::nullopt;
        }

        if (ImGui::BeginPopup("Output Popup"))
        {
            auto creating_connection_payload = DrawOutputOptions(ImVec2(200.0f, 30.0f));
            if (creating_connection_payload)
            {
                m_creating_connection_payload = creating_connection_payload;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

    } ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (!ImGui::IsMouseDown(0))
    {
        m_is_moving = false;
    }

    if (m_is_moving)
    {
        auto IO = ImGui::GetIO();
        OnPositionChange(ImGui::GetWindowPos(), ImGui::GetWindowPos() + IO.MouseDelta, IO.MouseDelta);
        m_window_pos = ImVec2(ImGui::GetWindowPos().x + IO.MouseDelta.x, ImGui::GetWindowPos().y + IO.MouseDelta.y);
    }
}

void Jani::Inspector::BaseWindow::ProcessResize()
{
    if (ImGui::IsMouseClicked(0) && ImGui::IsMouseHoveringWindow())
    {
        float resize_bar_length = 10.0f;

        auto mouse_position = ImGui::GetMousePos();
        auto left_pos_diff   = mouse_position.x - m_window_pos.x;
        auto top_pos_diff    = mouse_position.y - m_window_pos.y;
        auto right_pos_diff  = (m_window_pos.x + m_window_size.x) - mouse_position.x;
        auto bottom_pos_diff = (m_window_pos.y + m_window_size.y) - mouse_position.y;

        if (left_pos_diff > 0.f && left_pos_diff < resize_bar_length)
        {
            m_is_resizing[0] = true;
        }
        if (top_pos_diff > 0.f && top_pos_diff < resize_bar_length)
        {
            // m_is_resizing[1] = true;
        }
        if (right_pos_diff > 0.f && right_pos_diff < resize_bar_length)
        {
            m_is_resizing[2] = true;
        }
        if (bottom_pos_diff > 0.f && bottom_pos_diff < resize_bar_length)
        {
            m_is_resizing[3] = true;
        }
    }

    if (!ImGui::IsMouseDown(0))
    {
        m_is_resizing[0] = false;
        m_is_resizing[1] = false;
        m_is_resizing[2] = false;
        m_is_resizing[3] = false;
    }

    /*
    if (m_is_resizing[0])
    {
        OnSizeChange(m_window_size, m_window_size - ImVec2(ImGui::GetIO().MouseDelta.x, 0.0f), ImVec2(-ImGui::GetIO().MouseDelta.x, 0.0f));
        m_window_pos.x  += ImGui::GetIO().MouseDelta.x;
        m_window_size.x -= ImGui::GetIO().MouseDelta.x;
    }
    */

    /*
    if (m_is_resizing[1])
    {
        m_window_pos.y  -= ImGui::GetIO().MouseDelta.y;
        m_window_size.y += ImGui::GetIO().MouseDelta.y;
    }
    */

    if (m_is_resizing[2])
    {
        OnSizeChange(m_window_size, m_window_size + ImVec2(ImGui::GetIO().MouseDelta.x, 0.0f), ImVec2(ImGui::GetIO().MouseDelta.x, 0.0f));
        m_window_size.x += ImGui::GetIO().MouseDelta.x;
    }

    if (m_is_resizing[3])
    {
        OnSizeChange(m_window_size, m_window_size + ImVec2(0.0f, ImGui::GetIO().MouseDelta.y), ImVec2(0.0f, ImGui::GetIO().MouseDelta.y));
        m_window_size.y += ImGui::GetIO().MouseDelta.y;
    }
}

ImVec2 Jani::Inspector::BaseWindow::GetConnectionInputPosition() const
{
    return m_inputs_position;
}

ImVec2 Jani::Inspector::BaseWindow::GetConnectionOutputPosition() const
{
    return m_outputs_position;
}

std::optional<Jani::Inspector::WindowInputConnection> Jani::Inspector::BaseWindow::GetCreatingConnectionPayload() const
{
    return m_creating_connection_payload;
}

void Jani::Inspector::BaseWindow::ResetIsCreatingConnection()
{
    m_creating_connection_payload = std::nullopt;
}

bool Jani::Inspector::BaseWindow::AddWindowConnection(BaseWindow& _from, BaseWindow& _to, WindowDataType _data_type, WindowConnectionType _connection_type)
{
    auto data_enum_index = magic_enum::enum_index(_data_type);
    if (!data_enum_index)
    {
        return false;
    }

    if (IsConnectedRecursively(_to, _from))
    {
        return false;
    }

    if (_to.m_input_connections[data_enum_index.value()] != std::nullopt
        || _from.m_output_connections[data_enum_index.value()].find(_to.GetId()) != _from.m_output_connections[data_enum_index.value()].end())
    {
        return false;
    }

    _to.m_input_connections[data_enum_index.value()] = { &_from, _data_type, _connection_type };
    _from.m_output_connections[data_enum_index.value()].insert({ _to.GetId(),&_to });

    return true;
}

bool Jani::Inspector::BaseWindow::IsConnectedRecursively(BaseWindow& _current_window, BaseWindow& _target)
{
    for (auto& output_connections : _current_window.m_output_connections)
    {
        for (auto& [window_id, window] : output_connections)
        {
            if (window_id == _target.GetId())
            {
                return true;
            }

            if (IsConnectedRecursively(*window, _target))
            {
                return true;
            }
        }
    }

    return false;
}

bool Jani::Inspector::BaseWindow::RemoveWindowConnection(BaseWindow& _window, WindowDataType _data_type)
{
    auto data_enum_index = magic_enum::enum_index(_data_type);
    if (!data_enum_index)
    {
        return false;
    }

    if (_window.m_input_connections[data_enum_index.value()] == std::nullopt)
    {
        return false;
    }

    _window.m_input_connections[data_enum_index.value()].value().window->m_output_connections[data_enum_index.value()].erase(_window.GetId());
    _window.m_input_connections[data_enum_index.value()] = std::nullopt;

    return true;
}

void Jani::Inspector::BaseWindow::RemoveAllInputWindowConnections(BaseWindow& _window)
{
    for (int i = 0; i < magic_enum::enum_count<WindowDataType>(); i++)
    {
        if (_window.m_input_connections[i].has_value())
        {
            RemoveWindowConnection(_window, magic_enum::enum_value<WindowDataType>(i));
        }
    }
}

void Jani::Inspector::BaseWindow::RemoveAllOutputWindowConnections(BaseWindow& _window)
{
    for (auto& output_connection_info : _window.m_output_connections)
    {
        for (auto& [window_id, window] : output_connection_info)
        {
            for (int i = 0; i < window->m_input_connections.size(); i++)
            {
                auto& other_window_input_connection = window->m_input_connections[i];
                if (other_window_input_connection && other_window_input_connection->window->GetId() == _window.GetId())
                {
                    other_window_input_connection = std::nullopt;
                }
            }
        }

        output_connection_info.clear();
    }
}