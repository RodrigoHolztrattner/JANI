////////////////////////////////////////////////////////////////////////////////
// Filename: QueryWindow.h
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
// Class name: QueryWindow
////////////////////////////////////////////////////////////////////////////////
class QueryWindow : public BaseWindow
{
    friend InspectorManager;

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    QueryWindow(InspectorManager& _inspector_manager);
    ~QueryWindow();

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
    * Returns the type of data this window is able to receive
    */
    WindowDataType GetInputTypes() const final;

    /*
    * Returns the type of data this window is able to output
    */
    WindowDataType GetOutputTypes() const final;

    /*
    * Request output data
    */
    std::optional<std::unordered_map<EntityId, EntityData>> GetOutputEntityData() const final;
    std::optional<std::vector<std::shared_ptr<Constraint>>> GetOutputConstraint() const final;

protected:

    /*
    * If this window registered itself to receive query data, this is the function that
    * will be called
    */
    void ReceiveEntityQueryResults(const ComponentQueryResultPayload& _query_payload);

    /*
    * Return the query payload associated with this query window
    */
    std::optional<std::pair<WorldPosition, ComponentQuery>> GetComponentQueryPayload() const;

////////////////////////
private: // VARIABLES //
////////////////////////

    std::unordered_map<EntityId, EntityData> m_entity_datas;
    std::shared_ptr<Constraint>              m_constraints;
    ComponentMask                            m_constraint_required_components;

    bool m_query_active = false;

    std::optional<std::shared_ptr<Constraint>*> m_add_new_constraint_target;
    std::optional<ComponentMask*>               m_component_mask_selector_target;
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)