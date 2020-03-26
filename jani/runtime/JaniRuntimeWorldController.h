////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntimeWorldController.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"

#include "JaniRuntimeWorkerReference.h" // TODO: Move to .cpp

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

class RuntimeDatabase;
class RuntimeBridge;
class LayerConfig;
class WorkerSpawnerConfig;

class DeploymentConfig;
class LayerConfig;

class RuntimeWorkerReference;

template <typename mType, uint32_t mBucketDimSize = 16>
class EntitySparseGrid;

////////////////////////////////////////////////////////////////////////////////
// Class name: RuntimeWorldController
////////////////////////////////////////////////////////////////////////////////
class RuntimeWorldController
{
    using CellOwnershipChangeCallback        = std::function<void(const std::map<EntityId, ServerEntity*>&, WorldCellCoordinates, LayerId, const RuntimeWorkerReference&, const RuntimeWorkerReference&)>;
    using EntityLayerOwnershipChangeCallback = std::function<void(const ServerEntity&, LayerId, const RuntimeWorkerReference&, const RuntimeWorkerReference&)>;
    using WorkerLayerRequestCallback         = std::function<void(LayerId)>;

    struct WorkerDensityKey
    {
        WorkerDensityKey() {}
        WorkerDensityKey(const WorkerCellsInfos& _worker_cells_infos) : global_entity_count(_worker_cells_infos.entity_count), worker_id(_worker_cells_infos.worker_instance->GetId()) {}
        bool operator() (const WorkerDensityKey& rhs) const
        {
            if (global_entity_count < rhs.global_entity_count) return true;
            if (global_entity_count > rhs.global_entity_count) return false;
            if (worker_id < rhs.worker_id) return true;

            return false;
        }

        bool operator()(WorkerDensityKey const& lhs, WorkerDensityKey const& rhs) const
        {
            if (lhs.global_entity_count < rhs.global_entity_count) return true;
            if (lhs.global_entity_count > rhs.global_entity_count) return false;
            if (lhs.worker_id < rhs.worker_id) return true;

            return false;
        }

        friend bool operator <(const WorkerDensityKey& lhs, const WorkerDensityKey& rhs) //friend claim has to be here
        {
            if (lhs.global_entity_count < rhs.global_entity_count) return true;
            if (lhs.global_entity_count > rhs.global_entity_count) return false;
            if (lhs.worker_id < rhs.worker_id) return true;

            return false;
        }

        uint32_t global_entity_count = 0;
        WorkerId worker_id           = std::numeric_limits<WorkerId>::max();
    };

    struct WorkerInfo
    {
        WorkerCellsInfos                worker_cells_infos;
        std::unique_ptr<RuntimeWorkerReference> worker_instance;

        WorkerDensityKey GetDensityKey() const
        {
            return WorkerDensityKey(worker_cells_infos);
        }
    };

    struct LayerInfo
    {
        bool                                                          user_layer                   = false;
        bool                                                          uses_spatial_area            = false;
        uint32_t                                                      maximum_entities_per_worker  = std::numeric_limits<uint32_t>::max();
        uint32_t                                                      maximum_workers              = std::numeric_limits<uint32_t>::max();
        std::unordered_map<WorkerId, WorkerInfo>                      worker_instances;
        LayerId                                                       layer_id                     = std::numeric_limits<LayerId>::max();
        std::map<WorkerDensityKey, WorkerInfo*>                       ordered_worker_density_info;
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    RuntimeWorldController(
        const DeploymentConfig& _deployment_config,
        const LayerConfig&  _layer_config);
    ~RuntimeWorldController();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    
    */
    bool Initialize();

    /*
    
    */
    void Update();

    /*
    *
    */
    bool AddWorkerForLayer(std::unique_ptr<RuntimeWorkerReference> _worker, LayerId _layer_id);

    /*
    
    */
    const WorldCellInfo& GetWorldCellInfo(WorldCellCoordinates _coordinates) const;

    const std::unordered_map<WorkerId, WorkerInfo>& GetWorkersInfosForLayer(LayerId _layer_id) const;

    /*
    * This function will check and request workers for layers that doesn't have workers
    * The intention here is to call this function on startup to make sure there is at least one
    * worker available of each used layer
    * This function will ignore layers that have the maximum number of workers set to 0
    */
    void ValidateLayersWithoutWorkers() const;

    /*
    * Insert an entity into the world grid
    */
    void InsertEntity(ServerEntity& _entity, WorldPosition _position);

    /*
    
    */
    void RemoveEntity(ServerEntity& _entity);

    /*
    
    */
    void AcknowledgeEntityPositionChange(ServerEntity& _entity, WorldPosition _new_position);

    /*
    
    */
    WorldCellCoordinates ConvertPositionIntoCellCoordinates(WorldPosition _world_position) const;
    WorldPosition ConvertCellCoordinatesIntoPosition(WorldPosition _cell_coordinates) const;

    /*
    
    */
    float ConvertWorldScalarIntoCellScalar(float _scalar) const;

    /*
    * Perform a query around a position with a radius, calling the callback for each selected entity 
    */
    void ForEachEntityOnRadius(WorldPosition _world_position, float _radius, std::function<void(EntityId, ServerEntity&, WorldCellCoordinates)> _callback) const;

    /*
    * Perform a query in a certain rect, calling the callback for each selected entity
    */
    void ForEachEntityOnRect(WorldRect _world_rect, std::function<void(EntityId, ServerEntity&, WorldCellCoordinates)> _callback) const;

private:

    /*
    * Make sure there is an allocated cell for the given coordinates
    * Also ensure there is a worker for each layer (whenever applicable) on the given cell
    */
    void SetupCell(WorldCellCoordinates _cell_coordinates);

    /*
    
    */
    void SetupWorkCellEntityInsertion(WorldCellInfo& _cell_info, std::optional<LayerId> _layer = std::nullopt);
    void SetupWorkCellEntityRemoval(WorldCellInfo& _cell_info, std::optional<LayerId> _layer = std::nullopt);

///////////////////////
public: // CALLBACKS //
///////////////////////

    /*
    * Register the necessary callbacks
    */
    void RegisterCellOwnershipChangeCallback(CellOwnershipChangeCallback _callback);
    void RegisterEntityLayerOwnershipChangeCallback(EntityLayerOwnershipChangeCallback _callback);
    void RegisterWorkerLayerRequestCallback(WorkerLayerRequestCallback _callback);

private:


    /*
    
    */
    void ApplySpatialBalance();

private:

    const DeploymentConfig& m_deployment_config;
    const LayerConfig&  m_layer_config;

    std::array<std::optional<LayerInfo>, MaximumLayers> m_layer_infos;

    std::unique_ptr<EntitySparseGrid<WorldCellInfo>> m_world_grid;

    CellOwnershipChangeCallback        m_cell_ownership_change_callback;
    EntityLayerOwnershipChangeCallback m_entity_layer_ownership_change_callback;
    WorkerLayerRequestCallback         m_worker_layer_request_callback;
};

// Jani
JaniNamespaceEnd(Jani)