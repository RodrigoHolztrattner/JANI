////////////////////////////////////////////////////////////////////////////////
// Filename: MainWindow.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"
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

    MainWindow();
    ~MainWindow();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize the inspector manager
    */
    bool Initialize();

    /*
    
    */
    void Draw(
        uint32_t             _window_width, 
        uint32_t             _window_height, 
        const CellsInfos&    _cell_infos,
        const EntitiesInfos& _entities_infos, 
        const WorkersInfos&  _workers_infos);

////////////////////////
private: // VARIABLES //
////////////////////////

    float m_left_panel_splitter  = 300.0f;
    float m_right_panel_splitter = 800.0f;

    MapWindow      m_map_window;
    PropertyWindow m_property_window;
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)