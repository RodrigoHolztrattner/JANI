////////////////////////////////////////////////////////////////////////////////
// Filename: MapWindow.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"
#include "BaseWindow.h"
#include "..\InspectorCommon.h"
#include "..\imgui_extension.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

// Inspector
JaniNamespaceBegin(Inspector)

////////////////////////////////////////////////////////////////////////////////
// Class name: MapWindow
////////////////////////////////////////////////////////////////////////////////
class MapWindow : public BaseWindow
{
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    MapWindow(InspectorManager& _inspector_manager);
    ~MapWindow();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize the inspector manager
    */
    bool Initialize() final;

    /*
    * The main update function
    * This will always be called before the draw function (if it was set to be called
    * on the current frame)
    */
    void Update() final;

    /*
    * Render this window
    */
    void Draw(
        const CellsInfos&   _cell_infos,
        const WorkersInfos& _workers_infos) final;

    /*
    * Draw the input/output options for this window
    * Return true if an option was selected
    */
    std::optional<WindowInputConnection> DrawOutputOptions(ImVec2 _button_size) final;

    /*
    * Returns true if this window accept the given connection as an input
    */
    bool CanAcceptInputConnection(const WindowInputConnection& _connection) final;

    /*
    * Request output data
    */
    std::optional<std::unordered_map<EntityId, EntityData>>    GetOutputEntityData() const final;
    std::optional<std::vector<EntityId>>                       GetOutputEntityId()   const final;
    std::optional<std::vector<std::shared_ptr<Constraint>>>                     GetOutputConstraint() const final;
    std::optional<std::vector<WorldPosition>>                  GetOutputPosition()   const final;
    std::optional<WorldRect> GetOutputRect() const final { return m_map_bounding_box; }

    /*
    * Called when a delta is added into this window position
    */
    void OnPositionChange(ImVec2 _current_pos, ImVec2 _new_pos, ImVec2 _delta) final;

    /*
    * Called when a delta is added into this window size
    */
    void OnSizeChange(ImVec2 _current_size, ImVec2 _new_size, ImVec2 _delta) final;

////////////////////////
private: // VARIABLES //
////////////////////////

    float  m_zoom_level  = 5.0f;
    ImVec2 m_scroll      = ImVec2(0, 0);
    ImVec2 m_pure_scroll = ImVec2(0, 0);
    bool m_is_scrolling = false;

    std::unordered_map<EntityId, EntityData> m_entity_datas;
    std::vector<EntityId>                    m_entity_ids;
    std::optional<VisualizationSettings>     m_visualization_settings;

    WorldRect m_map_bounding_box;
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)