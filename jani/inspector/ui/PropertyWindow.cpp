////////////////////////////////////////////////////////////////////////////////
// Filename: PropertyWindow.cpp
////////////////////////////////////////////////////////////////////////////////
#include "PropertyWindow.h"
#include "imgui.h"
#include "..\imgui_extension.h"

Jani::Inspector::PropertyWindow::PropertyWindow()
{
}

Jani::Inspector::PropertyWindow::~PropertyWindow()
{
}

bool Jani::Inspector::PropertyWindow::Initialize()
{

    return true;
}

void Jani::Inspector::PropertyWindow::Draw(
    uint32_t             _window_width,
    const CellsInfos&    _cell_infos,
    const EntitiesInfos& _entities_infos, 
    const WorkersInfos&  _workers_infos)
{
    std::function<void()> info_draw_function;

    if (ImGui::BeginChild("Property Window", ImVec2(_window_width, 0)))
    {
        float bottom_height = ImGui::GetWindowSize().y - m_top_height;
        ImGui::Splitter(false, 4.0f, &m_top_height, &bottom_height, 100, 100);

        if (ImGui::BeginChild("Objects Window", ImVec2(0, m_top_height)))
        {
            if (ImGui::BeginCollapsingHeaderColored("Workers", ImColor(ImVec4(0.4, 0.4, 0.4, 1.0))))
            {
                int worker_loop_index = 0;
                for (auto& [worker_id, worker_info] : _workers_infos)
                {
                    std::string worker_id_text             = std::string("Worker id: ") + std::to_string(worker_id);
                    std::string total_entities_text        = "Total entities: " + std::to_string(worker_info.total_entities);
                    std::string layer_id_text              = "Layer id: " + std::to_string(worker_info.layer_id);
                    std::string network_data_received_text = "Network data received p/ second: " + pretty_bytes(worker_info.total_network_data_received_per_second);
                    std::string network_data_sent_text     = "Network data sent p/ second: " + pretty_bytes(worker_info.total_network_data_sent_per_second);
                    std::string memory_allocations_text    = "Total memory allocations p/ second: " + pretty_bytes(worker_info.total_memory_allocations_per_second);
                    std::string memory_allocated_text      = "Total memory allocated p/ second: " + pretty_bytes(worker_info.total_memory_allocated_per_second);

                    ImGui::RadioButton(worker_id_text.c_str(), &m_selected_worker_index, worker_loop_index);
                    if(m_selected_worker_index == worker_loop_index)
                    {
                        info_draw_function =
                            [=]()
                            {     
                                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                                ImGui::Text(worker_id_text.c_str());
                                ImGui::Text(total_entities_text.c_str());
                                ImGui::Text(layer_id_text.c_str());
                                ImGui::Separator();
                                ImGui::Text(network_data_received_text.c_str());
                                ImGui::Text(network_data_sent_text.c_str());
                                ImGui::Separator();
                                ImGui::Text(memory_allocations_text.c_str());
                                ImGui::Text(memory_allocated_text.c_str());
                                ImGui::PopStyleColor();
                            };
                    }

                    worker_loop_index++;
                }

                ImGui::EndCollapsingHeaderColored();
            }

            ImGui::EndChild();
        }

        if (ImGui::BeginChild("Info Window"))
        {
            if (info_draw_function)
            {
                info_draw_function();
            }

            ImGui::EndChild();
        }

    } ImGui::EndChild();
}