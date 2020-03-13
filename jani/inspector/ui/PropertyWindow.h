////////////////////////////////////////////////////////////////////////////////
// Filename: PropertyWindow.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"
#include "BaseWindow.h"
#include "..\InspectorCommon.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

// Inspector
JaniNamespaceBegin(Inspector)

////////////////////////////////////////////////////////////////////////////////
// Class name: PropertyWindow
////////////////////////////////////////////////////////////////////////////////
class PropertyWindow : public BaseWindow
{
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    PropertyWindow(InspectorManager& _inspector_manager);
    ~PropertyWindow();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize the inspector manager
    */
    bool Initialize();

    /*
    * Render this window
    */
    void Draw(
        uint32_t             _window_width,
        const CellsInfos&    _cell_infos,
        const EntitiesInfos& _entities_infos, 
        const WorkersInfos&  _workers_infos);

////////////////////////
private: // VARIABLES //
////////////////////////

    float m_top_height            = 300.0f;
    int   m_selected_worker_index = -1;
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)