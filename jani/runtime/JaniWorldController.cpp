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
        LayerInfo layer_info;

        layer_info.user_layer                  = layer_collection_info.user_layer;
        layer_info.uses_spatial_area           = layer_collection_info.use_spatial_area;
        layer_info.maximum_entities_per_worker = layer_collection_info.maximum_entities_per_worker;
        layer_info.maximum_workers             = layer_collection_info.maximum_workers;
        layer_info.layer_id                    = layer_id_count++;

        m_layer_infos[layer_id] = std::move(layer_info);
    }
    
    m_world_grid = std::make_unique<SparseGrid<WorldCellInfo>>(m_deployment_config.GetMaximumWorldLength());

    return true;
}

void Jani::WorldController::Update()
{
    ValidateLayersWithoutWorkers();

    ApplySpatialBalance();

    // Balance each layer that doesn't use spatial area
    for (auto& layer_info : m_layer_infos)
    {
        if (!layer_info
            || layer_info->uses_spatial_area
            || layer_info->worker_instances.size() == 0
            || (layer_info->worker_instances.size() == 1 && layer_info->maximum_workers == 1)) // If there is one instance and the limit is
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
        if (!layer_info)
        {
            break;
        }

        if (layer_info->worker_instances.size() == 0 && layer_info->maximum_workers > 0)
        {
            m_worker_layer_request_callback(layer_info->layer_id);
        }
    }
}

std::vector<std::string> s_previous_commands;

void PushDebugCommand(std::string _command)
{
    //s_previous_commands.push_back(std::move(_command));
}

void Jani::WorldController::InsertEntity(Entity& _entity, WorldPosition _position)
{
    PushDebugCommand("InsertEntity");

    WorldCellCoordinates cell_coordinates = ConvertPositionIntoCellCoordinates(SanitizeWorldPosition(_position));

    SetupCell(cell_coordinates);

    auto& cell_info = m_world_grid->AtMutable(cell_coordinates);

    assert(cell_info.entities.find(_entity.GetId()) == cell_info.entities.end());

    _entity.SetWorldCellInfo(cell_info);

    // Do something about each worker that owns the given cell? (for each layer)

    cell_info.entities.insert({ _entity.GetId(), &_entity });

    SetupWorkCellEntityInsertion(cell_info);
}

void Jani::WorldController::RemoveEntity(Entity& _entity)
{
    PushDebugCommand("RemoveEntity");

    WorldCellCoordinates cell_coordinates = _entity.GetWorldCellCoordinates();
    auto&                cell_info        = m_world_grid->AtMutable(cell_coordinates);

    // Do something about each worker that owns the given cell? (for each layer)

    cell_info.entities.erase(_entity.GetId());

    SetupWorkCellEntityRemoval(cell_info);
}

void Jani::WorldController::SetupWorkCellEntityInsertion(WorldCellInfo& _cell_info)
{
    PushDebugCommand("SetupWorkCellEntityInsertion");

    for (auto& layer_info : m_layer_infos)
    {
        if (!layer_info)
        {
            break;
        }

        auto& current_worker_cells_infos = _cell_info.worker_cells_infos[layer_info->layer_id];
        auto extracted_worker_node       = layer_info->ordered_worker_density_info.extract(WorkerDensityKey(*current_worker_cells_infos));
        {
            extracted_worker_node.key().global_entity_count++;
            extracted_worker_node.mapped()->worker_cells_infos.entity_count++;
            extracted_worker_node.mapped()->worker_cells_infos.coordinates_owned.insert(_cell_info.cell_coordinates);
        }
        auto insert_iter = layer_info->ordered_worker_density_info.insert(std::move(extracted_worker_node));
    }
}

void Jani::WorldController::SetupWorkCellEntityRemoval(WorldCellInfo& _cell_info)
{
    PushDebugCommand("SetupWorkCellEntityRemoval");

    for (auto& layer_info : m_layer_infos)
    {
        if (!layer_info)
        {
            break;
        }

        // We don't care about layers that don't use spatial info
        if (!layer_info->uses_spatial_area)
        {
            continue;
        }

        auto& current_worker_cells_infos = _cell_info.worker_cells_infos[layer_info->layer_id];
        auto extracted_worker_node       = layer_info->ordered_worker_density_info.extract(WorkerDensityKey(*current_worker_cells_infos));
        {
            extracted_worker_node.key().global_entity_count--;
            extracted_worker_node.mapped()->worker_cells_infos.entity_count--;
        }
        auto insert_iter = layer_info->ordered_worker_density_info.insert(std::move(extracted_worker_node));
    }
}

void Jani::WorldController::AcknowledgeEntityPositionChange(Entity& _entity, WorldPosition _new_position)
{
    WorldCellCoordinates current_world_cell_coordinates = _entity.GetWorldCellCoordinates();
    WorldCellCoordinates new_world_cell_coordinates     = ConvertPositionIntoCellCoordinates(SanitizeWorldPosition(_new_position)); // TODO: Fill this and remember to add a little padding so entities won't be moving between workers all the time

    if (current_world_cell_coordinates == new_world_cell_coordinates)
    {
        return;
    }

    PushDebugCommand("AcknowledgeEntityPositionChange id{" + std::to_string(_entity.GetId()) + std::string("} from{") + std::to_string(current_world_cell_coordinates.x) + "," + std::to_string(current_world_cell_coordinates.y) + "}" +
    " to{" + std::to_string(new_world_cell_coordinates.x) + "," + std::to_string(new_world_cell_coordinates.y) + "}");

    SetupCell(new_world_cell_coordinates);

    auto& current_world_cell_info = m_world_grid->AtMutable(current_world_cell_coordinates);
    auto& new_world_cell_info     = m_world_grid->AtMutable(new_world_cell_coordinates);

    assert(current_world_cell_info.entities.find(_entity.GetId()) != current_world_cell_info.entities.end());
    assert(new_world_cell_info.entities.find(_entity.GetId()) == new_world_cell_info.entities.end());

    _entity.SetWorldCellInfo(new_world_cell_info);

    uint32_t current_size = current_world_cell_info.entities.size();
    uint32_t new_size = new_world_cell_info.entities.size();

    current_world_cell_info.entities.erase(_entity.GetId());
    new_world_cell_info.entities.insert({ _entity.GetId(), &_entity });

    assert(current_world_cell_info.entities.size() == current_size - 1);
    assert(new_world_cell_info.entities.size() == new_size + 1);

    for (auto& layer_info : m_layer_infos)
    {
        if (!layer_info)
        {
            break;
        }

        // We don't care about layers that don't use spatial info
        if (!layer_info->uses_spatial_area)
        {
            continue;
        }

        auto* current_layer_worker = current_world_cell_info.worker_cells_infos[layer_info->layer_id]->worker_instance;
        auto* new_layer_worker     = new_world_cell_info.worker_cells_infos[layer_info->layer_id]->worker_instance;

        assert(current_layer_worker);
        assert(new_layer_worker);

        /*
        * If both cells are owned by the same worker, we can just ignore this layer
        */
        if (current_layer_worker == new_layer_worker)
        {
            continue;
        }

        SetupWorkCellEntityInsertion(new_world_cell_info);
        SetupWorkCellEntityRemoval(current_world_cell_info);

        assert(m_entity_layer_ownership_change_callback);
        m_entity_layer_ownership_change_callback(
            _entity, 
            layer_info->layer_id, 
            *current_layer_worker, 
            *new_layer_worker);
    }
}

Jani::WorldPosition Jani::WorldController::SanitizeWorldPosition(WorldPosition _world_position) const
{


    return _world_position;
}

void Jani::WorldController::SetupCell(WorldCellCoordinates _cell_coordinates)
{
    if (m_world_grid->IsCellEmpty(_cell_coordinates))
    {
        m_world_grid->Set(_cell_coordinates, WorldCellInfo());
    }
    else
    {
        return;
    }

    PushDebugCommand("SetupCell pos{" + std::to_string(_cell_coordinates.x) + "," + std::to_string(_cell_coordinates.y) + "}");

    auto& cell_info            = m_world_grid->AtMutable(_cell_coordinates);
    cell_info.cell_coordinates = _cell_coordinates;

    for (int i = 0; i < cell_info.worker_cells_infos.size(); i++)
    {
        auto& layer_info = m_layer_infos[i];
        if (!layer_info || layer_info.value().user_layer)
        {
            break;
        }

        // This is a new cell, so there shouldn't be any worker that owns the cell
        assert(cell_info.worker_cells_infos[i] == nullptr);

        // We require at least one worker
        assert(layer_info.value().ordered_worker_density_info.size() > 0);

        /*
        * Update the ordered worker density info (both the key and the entity count on the worker cells infos)
        */
        auto extracted_worker_node = layer_info->ordered_worker_density_info.extract(layer_info->ordered_worker_density_info.begin());
        {
            extracted_worker_node.mapped()->worker_cells_infos.coordinates_owned.insert(_cell_coordinates);
        }
        auto insert_iter = layer_info->ordered_worker_density_info.insert(std::move(extracted_worker_node));

        // Make the cell point to the right worker cells infos
        cell_info.worker_cells_infos[i] = &insert_iter.position->second->worker_cells_infos;
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

Jani::WorldCellCoordinates Jani::WorldController::ConvertPositionIntoCellCoordinates(WorldPosition _world_position) const
{
    auto maximum_world_length = m_deployment_config.GetMaximumWorldLength();

    if (m_deployment_config.UsesCentralizedWorldOrigin())
    {
        _world_position.x += maximum_world_length / 2;
        _world_position.y += maximum_world_length / 2;
    }

    _world_position.x = std::min(_world_position.x, static_cast<int32_t>(maximum_world_length));
    _world_position.y = std::min(_world_position.y, static_cast<int32_t>(maximum_world_length));
    _world_position.x = std::max(_world_position.x, 0);
    _world_position.y = std::max(_world_position.y, 0);

    assert(m_deployment_config.GetMaximumWorldLength() % m_deployment_config.GetTotalGridsPerWorldLine() == 0);

    uint32_t cell_unit_length = m_deployment_config.GetMaximumWorldLength() / m_deployment_config.GetTotalGridsPerWorldLine();

    _world_position.x /= cell_unit_length;
    _world_position.y /= cell_unit_length;

    return _world_position;
}

Jani::WorldPosition Jani::WorldController::ConvertCellCoordinatesIntoPosition(WorldPosition _cell_coordinates) const
{
    assert(m_deployment_config.GetMaximumWorldLength() % m_deployment_config.GetTotalGridsPerWorldLine() == 0);

    uint32_t cell_unit_length = m_deployment_config.GetMaximumWorldLength() / m_deployment_config.GetTotalGridsPerWorldLine();

    _cell_coordinates.x *= cell_unit_length;
    _cell_coordinates.y *= cell_unit_length;

    auto maximum_world_length = m_deployment_config.GetMaximumWorldLength();

    if (m_deployment_config.UsesCentralizedWorldOrigin())
    {
        _cell_coordinates.x -= maximum_world_length / 2;
        _cell_coordinates.y -= maximum_world_length / 2;
    }

    return _cell_coordinates;
}

void Jani::WorldController::ApplySpatialBalance()
{
    for (auto& layer_info : m_layer_infos)
    {
        if (!layer_info)
        {
            break;
        }

        if (!layer_info->uses_spatial_area 
            || layer_info->worker_instances.size() == 0
            || (layer_info->worker_instances.size() == 1 && layer_info->maximum_workers == 1)) // If there is one instance and the limit is
            // one, no balance is required/possible
        {
            continue;
        }

        // Determine if there is a worker instance that needs its excess to be distributed over other
        // workers
        WorkerInfo* worker_instance_over_limit = nullptr;
        for (auto& [worker_id, worker_instance] : layer_info->worker_instances)
        {
            if (worker_instance.worker_cells_infos.entity_count >= layer_info->maximum_entities_per_worker)
            {
                // Add a chance to not detect the current worker and move to another that is also over its capacity
                // TODO: Add a better system to perform this
                if (rand() % 2 == 0)
                {
                    continue;
                }

                worker_instance_over_limit = &worker_instance;
                break;
            }
        }

        if (worker_instance_over_limit == nullptr)
        {
            continue;
        }

        auto& worker_cells_infos = worker_instance_over_limit->worker_cells_infos;

        bool too_many_entities_on_same_cell    = false;
        bool not_enough_space_on_other_workers = false;
        for (auto coordinates_iter = worker_cells_infos.coordinates_owned.begin(); coordinates_iter != worker_cells_infos.coordinates_owned.end(); )
        {
            auto  cell_coordinate = *coordinates_iter;
            auto& cell_info       = m_world_grid->AtMutable(cell_coordinate);

            assert(cell_info.worker_cells_infos[layer_info->layer_id]->worker_instance == worker_instance_over_limit->worker_instance.get());

            // Check if we reached our objective
            if (worker_cells_infos.entity_count < layer_info->maximum_entities_per_worker)
            {
                break;
            }

            // Is there at least one entity on this cell to give away?
            if (cell_info.entities.size() == 0)
            {
                ++coordinates_iter;
                continue;
            }

            // Check if this cell is over it's capacity, making impossible to move its entities to another
            // This basically means that this worker is unable to give away entities to lower the current 
            // usage
            // We will still continue to try to give away other cells from this worker
            if (cell_info.entities.size() >= layer_info->maximum_entities_per_worker)
            {
                too_many_entities_on_same_cell = true;
                ++coordinates_iter;
                continue;
            }

            /*
            * Go through each other worker and check if someone can assume the entities from this cell
            */
            bool did_give_away_cell = false;
            for (auto& [target_worker_density_key, target_worker_info] : layer_info->ordered_worker_density_info)
            {
                // Ignore self
                if (target_worker_info->worker_instance == worker_instance_over_limit->worker_instance)
                {
                    continue;
                }

                // Do not make the selected worker go over 70% of its capacity
                if (target_worker_info->worker_cells_infos.entity_count + cell_info.entities.size() >= static_cast<uint32_t>(layer_info->maximum_entities_per_worker * 0.7))
                {
                    continue;
                }

                WorkerDensityKey over_capacity_worker_density_key = WorkerDensityKey(*cell_info.worker_cells_infos[layer_info->layer_id]);

                target_worker_info->worker_cells_infos.entity_count         += cell_info.entities.size();
                worker_instance_over_limit->worker_cells_infos.entity_count -= cell_info.entities.size();

                assert(worker_instance_over_limit->worker_cells_infos.coordinates_owned.find(cell_coordinate) != worker_instance_over_limit->worker_cells_infos.coordinates_owned.end());
                assert(target_worker_info->worker_cells_infos.coordinates_owned.find(cell_coordinate) == target_worker_info->worker_cells_infos.coordinates_owned.end());

                // worker_instance_over_limit->worker_cells_infos.coordinates_owned.erase(cell_coordinate); This is done below, we are inside a loop!
                target_worker_info->worker_cells_infos.coordinates_owned.insert(cell_coordinate);

                {
                    auto over_capacity_extracted_worker_node = layer_info->ordered_worker_density_info.extract(over_capacity_worker_density_key);
                    auto target_extracted_worker_node        = layer_info->ordered_worker_density_info.extract(target_worker_density_key);
                    {
                        over_capacity_extracted_worker_node.key().global_entity_count = worker_instance_over_limit->worker_cells_infos.entity_count;
                        target_extracted_worker_node.key().global_entity_count        = target_worker_info->worker_cells_infos.entity_count;
                        // over_capacity_extracted_worker_node.mapped()->worker_cells_infos.coordinates_owned.insert(cell_coordinate);
                    }
                    layer_info->ordered_worker_density_info.insert(std::move(over_capacity_extracted_worker_node));
                    layer_info->ordered_worker_density_info.insert(std::move(target_extracted_worker_node));
                }

                // Tenho que tomar cuidado pra nao invalidar o target_worker_info ao mexer no ordered_worker_density_info

                cell_info.worker_cells_infos[layer_info->layer_id] = &target_worker_info->worker_cells_infos;

                assert(m_cell_ownership_change_callback);
                m_cell_ownership_change_callback(
                    cell_info.entities,
                    cell_info.cell_coordinates,
                    layer_info->layer_id,
                    *worker_instance_over_limit->worker_instance,
                    *target_worker_info->worker_instance);

                did_give_away_cell = true;
                break;
            }

            not_enough_space_on_other_workers |= !did_give_away_cell;
            
            if (did_give_away_cell)
            {
                coordinates_iter = worker_cells_infos.coordinates_owned.erase(coordinates_iter);
            }
            else 
            {
                ++coordinates_iter;
            }
        }

        // If the current worker is still over its capacity 
        if (worker_cells_infos.entity_count >= layer_info->maximum_entities_per_worker
            && not_enough_space_on_other_workers)
        {
            assert(m_worker_layer_request_callback);
            m_worker_layer_request_callback(layer_info->layer_id);
        }
    }
}