////////////////////////////////////////////////////////////////////////////////
// Filename: MainWindow.cpp
////////////////////////////////////////////////////////////////////////////////
#include "MainWindow.h"
#include "imgui.h"
#include "..\imgui_extension.h"

#include "QueryWindow.h"
#include "MapWindow.h"
#include "PropertyWindow.h"

Jani::Inspector::MainWindow::MainWindow(InspectorManager& _inspector_manager) : m_inspector_manager(_inspector_manager)
{
}

Jani::Inspector::MainWindow::~MainWindow()
{
    m_child_windows.clear();
}

bool Jani::Inspector::MainWindow::Initialize()
{
    return true;
}

void Jani::Inspector::MainWindow::Update()
{
    for (auto& [window_id, child_window] : m_child_windows)
    {
        child_window->PreUpdate();
    }

    for (auto& [window_id, child_window] : m_child_windows)
    {
        child_window->Update();
    }
}

void Jani::Inspector::MainWindow::Draw(
    uint32_t            _window_width,
    uint32_t            _window_height,
    const CellsInfos&   _cell_infos,
    const WorkersInfos& _workers_infos)
{
    std::optional<Jani::Inspector::WindowInputConnection> creating_connection_payload;
    for (auto& [window_id, child_window] : m_child_windows)
    {
        auto current_creating_connection_payload = child_window->GetCreatingConnectionPayload();
        if (current_creating_connection_payload)
        {
            creating_connection_payload = current_creating_connection_payload;
            m_edit_mode                 = true;
        }
    }

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(_window_width, _window_height));
    if (ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        if (ImGui::BeginChild("Header", ImVec2(0, 30)))
        {
            if (ImGui::Button("Add Module"))
            {
                ImGui::OpenPopup("Module Creation Popup");
            }

            ImGui::SameLine();
            if (ImGui::Checkbox("Edit Mode", &m_edit_mode))
            {
            }

            if (ImGui::BeginPopup("Module Creation Popup"))
            {
                if (ImGui::Button("New Query Window"))
                {
                    auto new_query_window = std::make_unique<QueryWindow>(m_inspector_manager);
                    auto new_window_id = new_query_window->GetId();

                    m_child_windows.insert({ new_window_id, std::move(new_query_window) });

                    ImGui::CloseCurrentPopup();
                }

                if (ImGui::Button("New Map Window"))
                {
                    auto new_query_window = std::make_unique<MapWindow>(m_inspector_manager);
                    auto new_window_id = new_query_window->GetId();

                    m_child_windows.insert({ new_window_id, std::move(new_query_window) });

                    ImGui::CloseCurrentPopup();
                }

                ImGui::EndPopup();
            }

        } ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        for (auto& [window_id, child_window] : m_child_windows)
        {
            ImGui::SetNextWindowPos(child_window->GetWindowPos());

            if (ImGui::BeginChild(child_window->GetUniqueName().c_str(), child_window->GetWindowSize(), false, ImGuiWindowFlags_NoScrollbar))
            {
                child_window->DrawTitleBar(creating_connection_payload);

                if (child_window->IsVisible())
                {
                    if (ImGui::BeginChild("Internal"), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)
                    {
                        child_window->Draw(_cell_infos, _workers_infos);

                        child_window->ProcessResize();

                    } ImGui::EndChild();
                }

            } ImGui::EndChild();
        }
        ImGui::PopStyleColor(5);

        for (auto& [window_id, child_window] : m_child_windows)
        {
            if (!ImGui::IsMouseDown(0))
            {
                child_window->ResetIsCreatingConnection();
            }

            if (m_edit_mode)
            {
                child_window->DrawConnections();
            }
        }

        ImGui::End();
    }
    ImGui::PopStyleColor(3);
}