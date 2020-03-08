////////////////////////////////////////////////////////////////////////////////
// Filename: MainWindow.cpp
////////////////////////////////////////////////////////////////////////////////
#include "MainWindow.h"
#include "imgui.h"
#include "..\imgui_extension.h"

Jani::Inspector::MainWindow::MainWindow()
{
}

Jani::Inspector::MainWindow::~MainWindow()
{
}

bool Jani::Inspector::MainWindow::Initialize()
{
    if (!m_map_window.Initialize())
    {
        return false;
    }

    if (!m_property_window.Initialize())
    {
        return false;
    }

    return true;
}

void Jani::Inspector::MainWindow::Draw(
    uint32_t             _window_width,
    uint32_t             _window_height,
    const CellsInfos&    _cell_infos,
    const EntitiesInfos& _entities_infos, 
    const WorkersInfos&  _workers_infos)
{
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(_window_width, _window_height));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));

    if (ImGui::Begin("Main Window", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar))
    {
        float remaining_size_for_right = _window_width - m_left_panel_splitter;
        ImGui::Splitter(true, 4.0f, &m_left_panel_splitter, &remaining_size_for_right, 200, 200);

        if (ImGui::BeginChild("Left Window", ImVec2(m_left_panel_splitter, 0)))
        {

        } ImGui::EndChild();

        ImGui::SameLine();
        if (ImGui::BeginChild("Playground", ImVec2(remaining_size_for_right, 0)))
        {
            remaining_size_for_right = ImGui::GetWindowSize().x - m_right_panel_splitter;
            ImGui::Splitter(true, 4.0f, &m_right_panel_splitter, &remaining_size_for_right, 200, 200);

            m_map_window.Draw(m_right_panel_splitter, _cell_infos, _entities_infos);

            ImGui::SameLine();
            m_property_window.Draw(remaining_size_for_right, _cell_infos, _entities_infos, _workers_infos);

        } ImGui::EndChild();
        
        ImGui::End();
    }
    ImGui::PopStyleColor(3);
}