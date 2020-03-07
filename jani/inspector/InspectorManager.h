////////////////////////////////////////////////////////////////////////////////
// Filename: Inspector.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"
#include "imgui.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

// Inspector
JaniNamespaceBegin(Inspector)

class Renderer;

////////////////////////////////////////////////////////////////////////////////
// Class name: Runtime
////////////////////////////////////////////////////////////////////////////////
class InspectorManager
{
    struct EntityInfo
    {
        EntityId      id;
        WorldPosition world_position;
        WorkerId      worker_id;

        std::chrono::time_point<std::chrono::steady_clock> last_update_received_timestamp = std::chrono::steady_clock::now();
    };

    /*
    
auto time_from_last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
                auto time_elapsed_for_ping_ms = time_from_last_receive_ms - time_from_last_update_ms;
                std::chrono::time_point<std::chrono::steady_clock> m_last_update_timestamp = std::chrono::steady_clock::now();

    
    */
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    InspectorManager();
    ~InspectorManager();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize the inspector manager
    */
    bool Initialize(std::string _runtime_ip, uint32_t _runtime_port);

    /*
    * The main update function
    */
    void Update(uint32_t _time_elapsed_ms);

    /*
    * Returns if the inspector should disconnect
    */
    bool ShouldDisconnect() const;

////////////////////////
private: // VARIABLES //
////////////////////////

    std::unique_ptr<Jani::Connection<>>                m_runtime_connection;
    std::unique_ptr<Jani::RequestManager>              m_request_manager;
    std::chrono::time_point<std::chrono::steady_clock> m_last_runtime_request_time = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> m_last_update_time          = std::chrono::steady_clock::now();

    std::unique_ptr<Renderer> m_renderer;

    bool m_should_disconnect = false;

    // Last runtime responses
    Jani::Message::RuntimeGetCellsInfosResponse   m_cells_infos;

    std::unordered_map<EntityId, EntityInfo> m_entities_infos;

    float  m_zoom_level = 5.0f;
    ImVec2 m_scroll = ImVec2(0, 0);
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)