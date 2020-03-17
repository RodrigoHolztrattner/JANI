////////////////////////////////////////////////////////////////////////////////
// Filename: BaseWindow.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"
#include "..\InspectorCommon.h"
#include <imgui.h>

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

// Inspector
JaniNamespaceBegin(Inspector)

////////////////////////////////////////////////////////////////////////////////
// Class name: BaseWindow
////////////////////////////////////////////////////////////////////////////////
class BaseWindow
{
    friend MainWindow;

protected:

    struct ConnectionType
    {

    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    BaseWindow(InspectorManager& _inspector_manager, const std::string _window_type_name);
    virtual ~BaseWindow();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize the inspector manager
    */
    virtual bool Initialize() = 0;

    /*
    * Called right before calling the main Update function
    * This will reset the updated bool variable
    */
    void PreUpdate();

    /*
    * The main update function
    * This will always be called before the draw function (if it was set to be called
    * on the current frame)
    */
    virtual void Update();

    /*
    * Render this window
    */
    virtual void Draw(
        const CellsInfos&   _cell_infos,
        const WorkersInfos& _workers_infos);

    /*
    * Draw all connections that output from this window
    */
    void DrawConnections();

    /*
    * Draw the output options for this window
    * Returns true if an option was selected
    */
    virtual std::optional<WindowInputConnection> DrawOutputOptions(ImVec2 _button_size);

    /*
    * Returns true if this window accept the given connection as an input
    */
    virtual bool CanAcceptInputConnection(const WindowInputConnection& _connection);

    /*
    * Returns the type of data this window is able to receive
    */
    virtual WindowDataType GetInputTypes() const;

    /*
    * Returns the type of data this window is able to output
    */
    virtual WindowDataType GetOutputTypes() const;

    /*
    * Request output data
    */
    virtual std::optional<std::unordered_map<EntityId, EntityData>>            GetOutputEntityData()            const;
    virtual std::optional<std::vector<EntityId>>              GetOutputEntityId()              const;
    virtual std::optional<std::vector<std::shared_ptr<Constraint>>>            GetOutputConstraint()            const;
    virtual std::optional<std::vector<WorldPosition>>         GetOutputPosition()              const;
    virtual std::optional<VisualizationSettings> GetOutputVisualizationSettings() const;
    virtual std::optional<WorldRect> GetOutputRect() const { return std::nullopt; }

    /*
    * Called when a delta is added into this window position
    */
    virtual void OnPositionChange(ImVec2 _current_pos, ImVec2 _new_pos, ImVec2 _delta);

    /*
    * Called when a delta is added into this window size
    */
    virtual void OnSizeChange(ImVec2 _current_size, ImVec2 _new_size, ImVec2 _delta);

    /*
    * Return this window unique id
    */
    WindowId GetId() const;

    /*
    * Returns if this window was updated
    */
    bool WasUpdated() const;

    /*
    * Return this window position
    */
    ImVec2 GetWindowPos() const;

    /*
    * Return this window size
    */
    ImVec2 GetWindowSize() const;

protected:

    /*
    * Return this window unique name
    */
    const std::string GetUniqueName() const;

    /*
    * Return if this window is visible
    */
    bool IsVisible() const;

    /*
    * Draw this window title bar
    * Only call this when inside a child window
    */
    void DrawTitleBar(bool _edit_mode, std::optional<WindowInputConnection> _connection = std::nullopt);

    /*
    * Process any requested resize
    * Must be called when inside a child window
    */
    void ProcessResize();

    /*
    * Return the connection input/output origin position
    */
    ImVec2 GetConnectionInputPosition()  const;
    ImVec2 GetConnectionOutputPosition() const;

    /*
    * Return if this window is creating a connection and the payload associated
    */
    std::optional<WindowInputConnection> GetCreatingConnectionPayload() const;

    /*
    * Reset the status if this window is creating a connection
    */
    void ResetIsCreatingConnection();

    /*
    * Add a connection between windows
    */
    static bool AddWindowConnection(BaseWindow& _from, BaseWindow& _to, WindowDataType _data_type, WindowConnectionType _connection_type);

    /*
    * Checks if a given window is connected recursively with another
    */
    static bool IsConnectedRecursively(BaseWindow& _current_window, BaseWindow& _target);

    /*
    * Remove a connection for a given window (removes an input connection)
    */
    static bool RemoveWindowConnection(BaseWindow& _window, WindowDataType _data_type);

    /*
    * Remove all connections for a given window (removes both input and output connections)
    */
    static void RemoveAllInputWindowConnections(BaseWindow& _window);
    static void RemoveAllOutputWindowConnections(BaseWindow& _window);

//////////////////////////
protected: // VARIABLES //
//////////////////////////

    InspectorManager& m_inspector_manager;

    std::array<std::optional<WindowInputConnection>, magic_enum::enum_count<WindowDataType>()>      m_input_connections;
    std::array<std::unordered_map<WindowId, BaseWindow*>, magic_enum::enum_count<WindowDataType>()> m_output_connections;

    std::string m_name;
    std::string m_unique_name;
    ImVec2 m_window_pos  = ImVec2(200, 200);
    ImVec2 m_window_size = ImVec2(500, 300);

private:

    WindowId m_unique_id     = 0;
    bool     m_updated       = false;
    bool     m_is_moving     = false;
    bool     m_is_visible    = true;
    float    m_header_height = 30.0f;
    std::array<bool, 4> m_is_resizing = {};

    ImVec2 m_inputs_position        = ImVec2(0.0f, 0.0f);
    ImVec2 m_outputs_position       = ImVec2(0.0f, 0.0f);
    std::optional<WindowInputConnection> m_creating_connection_payload;

    std::optional<WindowInputConnection> m_receiving_connection;
};

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)