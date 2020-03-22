////////////////////////////////////////////////////////////////////////////////
// Filename: InspectorCommon.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

// Inspector
JaniNamespaceBegin(Inspector)

class InspectorManager;
class MainWindow;
class BaseWindow;
class QueryWindow;

using WindowId = uint32_t;

enum class WindowDataType
{
    None                  = 0, 
    EntityData            = 1 << 0, 
    EntityId              = 1 << 1,
    Constraint            = 1 << 2, 
    Position              = 1 << 3, 
    VisualizationSettings = 1 << 4, 
};

enum class WindowConnectionType
{
    None = 0,
    Selection, 
    Viewport,
    ViewportRect, 
    All,
    BoxConstraint, 
    Constraint,
};

struct WindowInputConnection
{
    BaseWindow*          window          = nullptr;
    WindowDataType       data_type       = WindowDataType::None;
    WindowConnectionType connection_type = WindowConnectionType::None;
};

struct EntityData
{
    EntityId      entity_id = std::numeric_limits<EntityId>::max();
    ComponentMask component_mask;
    WorldPosition world_position;

    std::chrono::time_point<std::chrono::steady_clock> last_update_received_timestamp = std::chrono::steady_clock::now();

    std::array<std::optional<ComponentPayload>, MaximumEntityComponents> component_payloads;
};

struct Constraint
{
    std::optional<WorldRect>     box_constraint;
    std::optional<WorldArea>     area_constraint;
    std::optional<float>         radius_constraint;
    std::optional<ComponentMask> component_constraint;

    std::optional<std::pair<std::shared_ptr<Constraint>, std::shared_ptr<Constraint>>> or_constraint;
    std::optional<std::pair<std::shared_ptr<Constraint>, std::shared_ptr<Constraint>>> and_constraint;
};

struct VisualizationSettings
{

};

struct EntityInfo
{
    EntityId                             id             = std::numeric_limits<EntityId>::max();
    WorldPosition                        world_position;
    WorkerId                             worker_id      = std::numeric_limits<WorkerId>::max();
    std::bitset<MaximumEntityComponents> component_mask;
    std::chrono::time_point<std::chrono::steady_clock> last_update_received_timestamp = std::chrono::steady_clock::now();
};

struct CellInfo
{
    WorkerId             worker_id      = std::numeric_limits<WorkerId>::max();
    LayerId              layer_id       = std::numeric_limits<LayerId>::max();
    WorldRect            rect;
    WorldPosition        position;
    WorldCellCoordinates coordinates;
    uint32_t             total_entities = std::numeric_limits<uint32_t>::max();

    std::chrono::time_point<std::chrono::steady_clock> last_update_received_timestamp = std::chrono::steady_clock::now();
};

struct WorkerInfo
{
    WorkerId worker_id      = std::numeric_limits<WorkerId>::max();
    uint32_t total_entities = 0;
    LayerId  layer_id       = std::numeric_limits<LayerId>::max();

    uint64_t total_memory_allocations_per_second    = 0;
    uint64_t total_memory_allocated_per_second      = 0;
    uint64_t total_network_data_received_per_second = 0;
    uint64_t total_network_data_sent_per_second     = 0;

    std::chrono::time_point<std::chrono::steady_clock> last_update_received_timestamp = std::chrono::steady_clock::now();
};

using CellsInfos    = std::unordered_map<WorldCellCoordinates, CellInfo, WorldCellCoordinatesHasher, WorldCellCoordinatesComparator>;
using EntitiesInfos = std::unordered_map<EntityId, EntityInfo>;
using WorkersInfos  = std::unordered_map<WorkerId, WorkerInfo>;

/*
    
auto time_from_last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_update_timestamp).count();
            auto time_elapsed_for_ping_ms = time_from_last_receive_ms - time_from_last_update_ms;
            std::chrono::time_point<std::chrono::steady_clock> m_last_update_timestamp = std::chrono::steady_clock::now();

    
*/

// Inspector
JaniNamespaceEnd(Inspector)

// Jani
JaniNamespaceEnd(Jani)