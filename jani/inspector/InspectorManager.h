////////////////////////////////////////////////////////////////////////////////
// Filename: Inspector.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"
#include "config/JaniLayerConfig.h"
#include "imgui.h"
#include "ui/MainWindow.h"

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
    friend QueryWindow;

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    InspectorManager(const Jani::LayerConfig& _layer_collection);
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

    /*
    * Return a reference to the layer collection
    */
    const Jani::LayerConfig& GetLayerCollection() const;

protected:

    /*
    * Register a window to receive custom queries
    */
    void RegisterQueryWindow(QueryWindow& _window);

    /*
    * Unregister a previous registered query window
    */
    void UnregisterQueryWindow(QueryWindow& _window);

////////////////////////
private: // VARIABLES //
////////////////////////

    const Jani::LayerConfig& m_layer_collection;

    std::unique_ptr<Jani::Connection<>>                m_runtime_connection;
    std::unique_ptr<Jani::RequestManager>              m_request_manager;
    std::chrono::time_point<std::chrono::steady_clock> m_last_runtime_request_time = std::chrono::steady_clock::now();
    std::chrono::time_point<std::chrono::steady_clock> m_last_update_time          = std::chrono::steady_clock::now();

    std::unique_ptr<Renderer>   m_renderer;
    std::unique_ptr<MainWindow> m_main_window;

    std::unordered_map<WindowId, QueryWindow*> m_query_windows;

    bool m_should_disconnect = false;

    CellsInfos    m_cell_infos;
    EntitiesInfos m_entities_infos;
    WorkersInfos  m_workers_infos;
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)