////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorldController.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"

#include "JaniWorkerInstance.h" // TODO: Move to .cpp

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

class Database;
class Bridge;
class LayerCollection;
class WorkerSpawnerCollection;

class DeploymentConfig;
class LayerCollection;

class WorkerInstance;

////////////////////////////////////////////////////////////////////////////////
// Class name: WorldController
////////////////////////////////////////////////////////////////////////////////
class WorldController
{
    using CellOwnershipChangeCallback        = std::function<void(WorldCellCoordinates, LayerId, WorkerId, WorkerId)>;
    using EntityLayerOwnershipChangeCallback = std::function<void(const Entity&, LayerId, const WorkerInstance&, const WorkerInstance&)>;
    using WorkerLayerRequestCallback         = std::function<void(LayerId)>;

    struct WorkerDensityKey
    {
        bool operator() (const WorkerDensityKey& rhs) const
        {
            if (global_entity_count < rhs.global_entity_count) return true;
            if (global_entity_count > rhs.global_entity_count) return false;
            if (worker_id < rhs.worker_id) return true;

            return false;
        }

        uint32_t global_entity_count = 0;
        WorkerId worker_id           = std::numeric_limits<WorkerId>::max();
    };

    struct WorkerInfo
    {
        std::unique_ptr<WorkerInstance> worker_instance;
        std::set<WorldCellWorkerInfo>   ordered_cell_usage_info; // Set usado pra detectar quais sao as cells com menor entidades deste worker
        uint32_t                        global_entity_count = 0;

        WorkerDensityKey GetDensityKey() const
        {
            return { global_entity_count, worker_instance ->GetId()};
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

    WorldController(
        const DeploymentConfig& _deployment_config,
        const LayerCollection&  _layer_collection);
    ~WorldController();

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
    bool AddWorkerForLayer(std::unique_ptr<WorkerInstance> _worker, LayerId _layer);

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
    void InsertEntity(Entity& _entity, WorldPosition _position);

    /*
    
    */
    void RemoveEntity(Entity& _entity);

    /*
    
    */
    void AcknowledgeEntityPositionChange(Entity& _entity, WorldPosition _new_position);

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

    WorldCellCoordinates ConvertPositionIntoCellCoordinates(WorldPosition _world_position) const;

    /*
    
    */
    void ApplySpatialBalance();

private:

    const DeploymentConfig& m_deployment_config;
    const LayerCollection&  m_layer_collection;

    std::array<LayerInfo, MaximumLayers> m_layer_infos;

    std::unique_ptr<SparseGrid<WorldCellInfo>> m_world_grid;

    CellOwnershipChangeCallback        m_cell_ownership_change_callback;
    EntityLayerOwnershipChangeCallback m_entity_layer_ownership_change_callback;
    WorkerLayerRequestCallback         m_worker_layer_request_callback;
};

// Jani
JaniNamespaceEnd(Jani)