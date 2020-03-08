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
    using CellOwnershipChangeCallback        = std::function<void(const std::map<EntityId, Entity*>&, WorldCellCoordinates, LayerId, const WorkerInstance&, const WorkerInstance&)>;
    using EntityLayerOwnershipChangeCallback = std::function<void(const Entity&, LayerId, const WorkerInstance&, const WorkerInstance&)>;
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
        std::unique_ptr<WorkerInstance> worker_instance;

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
    bool AddWorkerForLayer(std::unique_ptr<WorkerInstance> _worker, LayerId _layer_id)
    {
        assert(_layer_id < MaximumLayers);
        auto& layer_info = m_layer_infos[_layer_id];
        if (!layer_info)
        {
            return false;
        }

        if (layer_info.value().worker_instances.size() + 1 > layer_info.value().maximum_workers)
        {
            return false;
        }

        WorkerInfo worker_info;
        worker_info.worker_cells_infos.entity_count    = 0;
        worker_info.worker_instance                    = std::move(_worker);
        worker_info.worker_cells_infos.worker_instance = worker_info.worker_instance.get();

        WorkerId worker_id = worker_info.worker_instance->GetId();
        auto density_key   = worker_info.GetDensityKey();

        auto insert_iter = layer_info.value().worker_instances.insert({worker_id, std::move(worker_info)});
        layer_info.value().ordered_worker_density_info.insert({ density_key, &insert_iter.first->second });

        return true;
    }

    /*
    
    */
    const WorldCellInfo& GetWorldCellInfo(WorldCellCoordinates _coordinates) const
    {
        return m_world_grid->At(_coordinates);
    }

    const std::unordered_map<WorkerId, WorkerInfo>& GetWorkersInfosForLayer(LayerId _layer_id) const
    {
        assert(_layer_id < MaximumLayers);
        if (m_layer_infos[_layer_id])
        {
            return m_layer_infos[_layer_id]->worker_instances;
        }
        else
        {
            static std::unordered_map<WorkerId, WorkerInfo> dummy;
            return dummy;
        }
    }

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

    /*
    
    */
    WorldCellCoordinates ConvertPositionIntoCellCoordinates(WorldPosition _world_position) const;
    WorldPosition ConvertCellCoordinatesIntoPosition(WorldPosition _cell_coordinates) const;

private:

    /*
    * Make sure there is an allocated cell for the given coordinates
    * Also ensure there is a worker for each layer (whenever applicable) on the given cell
    */
    void SetupCell(WorldCellCoordinates _cell_coordinates);

    /*
    
    */
    void SetupWorkCellEntityInsertion(WorldCellInfo& _cell_info, std::optional<uint32_t> _layer = std::nullopt);
    void SetupWorkCellEntityRemoval(WorldCellInfo& _cell_info, std::optional<uint32_t> _layer = std::nullopt);

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
    const LayerCollection&  m_layer_collection;

    std::array<std::optional<LayerInfo>, MaximumLayers> m_layer_infos;

    std::unique_ptr<SparseGrid<WorldCellInfo>> m_world_grid;

    CellOwnershipChangeCallback        m_cell_ownership_change_callback;
    EntityLayerOwnershipChangeCallback m_entity_layer_ownership_change_callback;
    WorkerLayerRequestCallback         m_worker_layer_request_callback;
};

// Jani
JaniNamespaceEnd(Jani)