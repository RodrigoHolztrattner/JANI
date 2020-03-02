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
    Jani::Message::RuntimeGetEntitiesInfoResponse m_entities_infos;
    Jani::Message::RuntimeGetWorkersInfoResponse  m_workers_infos;

    float  m_zoom_level = 5.0f;
    ImVec2 m_scroll = ImVec2(0, 0);
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)