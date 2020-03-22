////////////////////////////////////////////////////////////////////////////////
// Filename: MainWindow.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"
#include "..\InspectorCommon.h"
#include "MapWindow.h"
#include "PropertyWindow.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

// Inspector
JaniNamespaceBegin(Inspector)

////////////////////////////////////////////////////////////////////////////////
// Class name: MainWindow
////////////////////////////////////////////////////////////////////////////////
class MainWindow
{
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    MainWindow(InspectorManager& _inspector_manager);
    ~MainWindow();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize the inspector manager
    */
    bool Initialize();

    /*
    * Update this and all child windows
    */
    void Update();

    /*
    * Render this window
    */
    void Draw(
        uint32_t            _window_width, 
        uint32_t            _window_height, 
        const CellsInfos&   _cell_infos,
        const WorkersInfos& _workers_infos);

////////////////////////
private: // VARIABLES //
////////////////////////

    InspectorManager&                                         m_inspector_manager;
    std::unordered_map<WindowId, std::unique_ptr<BaseWindow>> m_child_windows;

    bool m_edit_mode = false;
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)