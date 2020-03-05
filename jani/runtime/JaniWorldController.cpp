////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorldController.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorldController.h"
#include "JaniDeploymentConfig.h"
#include "JaniLayerCollection.h"

Jani::WorldController::WorldController(
    const DeploymentConfig& _deployment_config,
    const LayerCollection&  _layer_collection) 
    : m_deployment_config(_deployment_config)
    , m_layer_collection(_layer_collection)
{

}

Jani::WorldController::~WorldController()
{
}

bool Jani::WorldController::Initialize()
{
    if (!m_deployment_config.IsValid()
        || !m_layer_collection.IsValid())
    {
        return false;
    }

    auto& layer_collection_infos = m_layer_collection.GetLayers();
    LayerId layer_id_count = 0;
    for (auto& [layer_id, layer_collection_info] : layer_collection_infos)
    {
        auto& layer_info = m_layer_infos[layer_id];

        layer_info.user_layer                  = layer_collection_info.user_layer;
        layer_info.uses_spatial_area           = layer_collection_info.use_spatial_area;
        layer_info.maximum_entities_per_worker = layer_collection_info.maximum_entities_per_worker;
        layer_info.maximum_workers             = layer_collection_info.maximum_workers;
        layer_info.layer_id                    = layer_id_count++;
    }

    return true;
}

void Jani::WorldController::Update()
{
    ValidateLayersWithoutWorkers();

    ApplySpatialBalance();

    // Balance each layer that doesn't use spatial area
    for (auto& layer_info : m_layer_infos)
    {
        if (layer_info.uses_spatial_area
            || layer_info.worker_instances.size() == 0
            || (layer_info.worker_instances.size() == 1 && layer_info.maximum_workers == 1)) // If there is one instance and the limit is
            // one, no balance is required/possible
        {
            continue;
        }

        // TODO: ...
    }
}

void Jani::WorldController::ValidateLayersWithoutWorkers() const
{
    for (auto& layer_info : m_layer_infos)
    {
        if (layer_info.worker_instances.size() == 0 && layer_info.maximum_workers > 0)
        {
            m_worker_layer_request_callback(layer_info.layer_id);
        }
    }
}

void Jani::WorldController::InsertEntity(Entity& _entity, WorldPosition _position)
{
    WorldCellCoordinates cell_coordinates = ConvertPositionIntoCellCoordinates(_position);
    auto&                cell_info        = m_world_grid->AtMutable(cell_coordinates);

    assert(cell_info.entities.find(_entity.GetId()) != cell_info.entities.end());
    assert(_entity.GetComponentMask().count() == 0); // Maybe this should not be a restriction?

    _entity.SetWorldCellInfo(cell_info);

    // Do something about each worker that owns the given cell? (for each layer)

    cell_info.entities.insert({ _entity.GetId(), &_entity });
}

void Jani::WorldController::RemoveEntity(Entity& _entity)
{
    WorldCellCoordinates cell_coordinates = _entity.GetWorldCellCoordinates();
    auto&                cell_info        = m_world_grid->AtMutable(cell_coordinates);

    // Do something about each worker that owns the given cell? (for each layer)

    cell_info.entities.erase(_entity.GetId());
}

void Jani::WorldController::AcknowledgeEntityPositionChange(Entity& _entity, WorldPosition _new_position)
{
    // Determine if this entity is entering a new cell
    // TODO: Check current cell and compare with the new position, if that represents
    // a different cell, update the entity current cell and proceed below
    // ...

    WorldCellCoordinates current_world_cell_coordinates = _entity.GetWorldCellCoordinates();
    WorldCellCoordinates new_world_cell_coordinates     = ConvertPositionIntoCellCoordinates(_new_position); // TODO: Fill this and remember to add a little padding so entities won't be moving between workers all the time

    if (current_world_cell_coordinates == new_world_cell_coordinates)
    {
        return;
    }

    auto& current_world_cell_info = m_world_grid->AtMutable(current_world_cell_coordinates);
    auto& new_world_cell_info     = m_world_grid->AtMutable(current_world_cell_coordinates);

    _entity.SetWorldCellInfo(new_world_cell_info);

    assert(current_world_cell_info.entities.find(_entity.GetId()) != current_world_cell_info.entities.end());
    assert(new_world_cell_info.entities.find(_entity.GetId()) == new_world_cell_info.entities.end());

    current_world_cell_info.entities.erase(_entity.GetId());
    new_world_cell_info.entities.insert({ _entity.GetId(), &_entity });

    for (auto& layer_info : m_layer_infos)
    {
        // We don't care about layers that don't use spatial info
        if (!layer_info.uses_spatial_area)
        {
            continue;
        }

        auto& current_cell_layer_info = current_world_cell_info.layer_infos[layer_info.layer_id];
        auto& new_cell_layer_info     = new_world_cell_info.layer_infos[layer_info.layer_id];

        auto* current_layer_worker = current_cell_layer_info.worker_instance;
        auto* new_layer_worker     = new_cell_layer_info.worker_instance;

        current_cell_layer_info.entity_count--; // TODO: Do something if it reaches 0?
        new_cell_layer_info.entity_count++;

        assert(m_entity_layer_ownership_change_callback);
        assert(new_layer_worker);
        assert(new_layer_worker);

        m_entity_layer_ownership_change_callback(
            _entity, 
            layer_info.layer_id, 
            *current_layer_worker, 
            *new_layer_worker);

    }
}

void Jani::WorldController::RegisterCellOwnershipChangeCallback(CellOwnershipChangeCallback _callback)
{
    m_cell_ownership_change_callback = _callback;
}

void Jani::WorldController::RegisterEntityLayerOwnershipChangeCallback(EntityLayerOwnershipChangeCallback _callback)
{
    m_entity_layer_ownership_change_callback = _callback;
}

void Jani::WorldController::RegisterWorkerLayerRequestCallback(WorkerLayerRequestCallback _callback)
{
    m_worker_layer_request_callback = _callback;
}

void Jani::WorldController::ApplySpatialBalance()
{
    for (auto& layer_info : m_layer_infos)
    {
        if (!layer_info.uses_spatial_area 
            || layer_info.worker_instances.size() == 0
            || (layer_info.worker_instances.size() == 1 && layer_info.maximum_workers == 1)) // If there is one instance and the limit is
            // one, no balance is required/possible
        {
            continue;
        }

        // If there is only one worker, check if its over capacity and a new worker is required (if its
        // possible to allocate a new one for this layer)
        if (layer_info.worker_instances.size() == 1
            && layer_info.worker_instances.begin()->second.global_entity_count > layer_info.maximum_entities_per_worker
            && layer_info.maximum_workers > 1)
        {
            // Flag that another worker needs to be created for this layer
            assert(m_worker_layer_request_callback);
            m_worker_layer_request_callback(layer_info.layer_id);

            continue;
        }

        // Determine if there is a worker instance that needs its surplus to be distributed over other
        // workers
        WorkerInfo* worker_instance_over_limit = nullptr;
        for (auto& [worker_id, worker_instance] : layer_info.worker_instances)
        {
            if (worker_instance.global_entity_count >= layer_info.maximum_entities_per_worker)
            {
                worker_instance_over_limit = &worker_instance;
                break;
            }
        }

        if (worker_instance_over_limit == nullptr)
        {
            continue;
        }

        /*
        * The idea is to move all the cells starting with the lowest density one to other workers
        * that can handle more entities
        * We keep doing this until the worker is not over its capacity or there are no workers
        * available to receive the extra entities, in this case we should try to create another
        * worker and try again on the next update
        */
        bool cannot_continue = false;
        for (auto cell_info_iter = worker_instance_over_limit->ordered_cell_usage_info.cbegin(); cell_info_iter != worker_instance_over_limit->ordered_cell_usage_info.cend();)
        {
            if (cannot_continue || worker_instance_over_limit->global_entity_count < layer_info.maximum_entities_per_worker)
            {
                break;
            }

            auto& cell_info = *cell_info_iter;

            /*
            * Find the worker that have the lowest amount of entities and try to give him the ownership of the
            * selected cell
            */
            bool cell_has_switched_ownership = false;
            for (auto& [density_key, worker_info] : layer_info.ordered_worker_density_info)
            {
                // Make sure this worker is not over its capacity and has enough space
                if (worker_info->global_entity_count + cell_info.entity_count > layer_info.maximum_entities_per_worker)
                {
                    // There is no point trying anymore, since the `ordered_grid_usage_info` is ordered by entity size
                    // this is guaranteed to be the worker with the lowest number of entities, if it failed it means
                    // no other worker will be able to accept the entities
                    // Basically we need to try to create another worker for this layer
                    cannot_continue = true;
                    break;
                }

                // Do not try to move to itself
                if (worker_info->worker_instance->GetId() == worker_instance_over_limit->worker_instance->GetId())
                {
                    continue;
                }

                // Remove old entries from the ordered_worker_density_info
                {
                    WorkerDensityKey over_limit_worker_key = { worker_instance_over_limit->global_entity_count, worker_instance_over_limit->worker_instance->GetId() };
                    WorkerDensityKey target_worker_key     = { worker_info->global_entity_count, worker_info->worker_instance->GetId() };

                    layer_info.ordered_worker_density_info.erase(over_limit_worker_key);
                    layer_info.ordered_worker_density_info.erase(target_worker_key);
                }

                // Update the entity count
                worker_instance_over_limit->global_entity_count -= cell_info.entity_count;
                worker_info->global_entity_count                += cell_info.entity_count;

                auto target_worker_info_iter    = layer_info.worker_instances.find(worker_info->worker_instance->GetId());
                assert(target_worker_info_iter != layer_info.worker_instances.end());
                auto* target_worker_info        = &target_worker_info_iter->second;

                // Add the new cell into the target worker so it can keep track of it
                // There is no need to remove the cell from the worker that is on limit because it will
                // be removed after we leave the current loop
                WorldCellWorkerInfo target_cell_info = cell_info;
                target_cell_info.worker_instance     = worker_info->worker_instance.get();
                target_cell_info.worker_id           = worker_info->worker_instance->GetId();
                target_worker_info->ordered_cell_usage_info.insert(std::move(target_cell_info));

                // Add new entries on the ordered_worker_density_info
                {
                    WorkerDensityKey new_over_limit_worker_key = { worker_instance_over_limit->global_entity_count, worker_instance_over_limit->worker_instance->GetId() };
                    WorkerDensityKey new_target_worker_key     = { worker_info->global_entity_count, worker_info->worker_instance->GetId() };
                        
                    layer_info.ordered_worker_density_info.insert({ new_over_limit_worker_key, worker_instance_over_limit });
                    layer_info.ordered_worker_density_info.insert({ new_target_worker_key, &target_worker_info_iter->second });
                }                    

                // Make the runtime aware that this ownership change is necessary
                {
                    WorldCellCoordinates cell_coordinates = cell_info.cell_coordinates;
                    WorkerId             from_worker      = worker_instance_over_limit->worker_instance->GetId();
                    WorkerId             to_worker        = worker_info->worker_instance->GetId();

                    assert(m_cell_ownership_change_callback);
                    m_cell_ownership_change_callback(cell_coordinates, layer_info.layer_id, from_worker, to_worker);
                }

                cell_has_switched_ownership = true;

                break;
            }

            if (cell_has_switched_ownership)
            {
                cell_info_iter = worker_instance_over_limit->ordered_cell_usage_info.erase(cell_info_iter);
            }
            else
            {
                ++cell_info_iter;
            }  
        }

        // If we were not able to balance the entities between workers, check if at least we can request to create another worker
        if (cannot_continue && layer_info.worker_instances.size() < layer_info.maximum_workers)
        {
            // Request to create another worker
            assert(m_worker_layer_request_callback);
            m_worker_layer_request_callback(layer_info.layer_id);
        }
    }
}