////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntimeWorldController.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntimeWorldController.h"
#include "JaniRuntimeEntitySparseGrid.h"
#include "config/JaniDeploymentConfig.h"
#include "config/JaniLayerConfig.h"

Jani::RuntimeWorldController::RuntimeWorldController(
    const DeploymentConfig& _deployment_config,
    const LayerConfig&  _layer_config) 
    : m_deployment_config(_deployment_config)
    , m_layer_config(_layer_config)
{

}

Jani::RuntimeWorldController::~RuntimeWorldController()
{
}

bool Jani::RuntimeWorldController::Initialize()
{
    if (!m_deployment_config.IsValid()
        || !m_layer_config.IsValid())
    {
        return false;
    }

    auto& layer_collection_infos = m_layer_config.GetLayers();
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
    
    m_world_grid = std::make_unique<EntitySparseGrid<WorldCellInfo>>(m_deployment_config.GetMaximumWorldLength());

    return true;
}

void Jani::RuntimeWorldController::Update()
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

    /* MOVED TO AddWorkerForLayer()
    // Perform dummy worker migrations, whenever required
    for (auto& layer_info : m_layer_infos)
    {
        if (layer_info->dummy_layer_worker_instance.worker_cells_infos.coordinates_owned.size() == 0
            || layer_info->ordered_worker_density_info.size() == 0)
        {
            continue;
        }

        for (auto& cell_coordinates : layer_info->dummy_layer_worker_instance.worker_cells_infos.coordinates_owned)
        {
            auto& cell_info = m_world_grid->AtMutable(cell_coordinates);

            auto& target_worker_info       = layer_info->ordered_worker_density_info.begin()->second;
            auto* dummy_worker_cells_infos = &layer_info->dummy_layer_worker_instance.worker_cells_infos;

            WorkerDensityKey target_worker_density_key = layer_info->ordered_worker_density_info.begin()->first;
            WorkerDensityKey over_capacity_worker_density_key = WorkerDensityKey(*cell_info.worker_cells_infos[layer_info->layer_id]);
            
            target_worker_info->worker_cells_infos.entity_count += cell_info.entities.size();
            dummy_worker_cells_infos->entity_count              -= cell_info.entities.size();

            dummy_worker_cells_infos->coordinates_owned.erase(cell_coordinates);
            target_worker_info->worker_cells_infos.coordinates_owned.insert(cell_coordinates);

            {
                auto over_capacity_extracted_worker_node = layer_info->ordered_worker_density_info.extract(over_capacity_worker_density_key);
                auto target_extracted_worker_node = layer_info->ordered_worker_density_info.extract(target_worker_density_key);
                {
                    over_capacity_extracted_worker_node.key().global_entity_count = dummy_worker_cells_infos->entity_count;
                    target_extracted_worker_node.key().global_entity_count = target_worker_info->worker_cells_infos.entity_count;
                    // over_capacity_extracted_worker_node.mapped()->worker_cells_infos.coordinates_owned.insert(cell_coordinates);
                }
                layer_info->ordered_worker_density_info.insert(std::move(over_capacity_extracted_worker_node));
                layer_info->ordered_worker_density_info.insert(std::move(target_extracted_worker_node));
            }

            cell_info.worker_cells_infos[layer_info->layer_id] = &target_worker_info->worker_cells_infos;

            assert(m_cell_ownership_change_callback);
            m_cell_ownership_change_callback(
                cell_info.entities,
                cell_info.cell_coordinates,
                layer_info->layer_id,
                nullptr,
                *target_worker_info->worker_instance);
        }
    }
    */ 
}

bool Jani::RuntimeWorldController::AddWorkerForLayer(std::unique_ptr<RuntimeWorkerReference> _worker, LayerId _layer_id)
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
    worker_info.worker_cells_infos.entity_count = 0;
    worker_info.worker_instance = std::move(_worker);
    worker_info.worker_cells_infos.worker_instance = worker_info.worker_instance.get();

    WorkerId worker_id = worker_info.worker_instance->GetId();
    auto density_key = worker_info.GetDensityKey();

    auto insert_iter = layer_info.value().worker_instances.insert({ worker_id, std::move(worker_info) });
    layer_info.value().ordered_worker_density_info.insert({ density_key, &insert_iter.first->second });

    //
    //
    //

    // Perform dummy worker migrations, whenever required
    if (layer_info->dummy_layer_worker_instance.worker_cells_infos.coordinates_owned.size() != 0)
    {
        for (auto& cell_coordinates : layer_info->dummy_layer_worker_instance.worker_cells_infos.coordinates_owned)
        {
            auto& cell_info = m_world_grid->AtMutable(cell_coordinates);

            MigrateEntitiesFromCellToNewWorker(
                layer_info.value(), 
                cell_info,
                layer_info->dummy_layer_worker_instance, 
                *layer_info->ordered_worker_density_info.begin()->second, 
                std::nullopt, 
                layer_info->ordered_worker_density_info.begin()->first, 
                false);
        }

        layer_info->dummy_layer_worker_instance.worker_cells_infos.coordinates_owned.clear();
    }

    return true;
}

bool Jani::RuntimeWorldController::HandleWorkerDisconnection(WorkerId _worker_id, LayerId _layer_id)
{
    assert(_layer_id < MaximumLayers);

    // If there is no info about this layer, there is nothing that must be done
    if (!m_layer_infos[_layer_id].has_value())
    {
        return true;
    }

    auto& layer_info = m_layer_infos[_layer_id].value();

    // User layers should be ignored
    if (layer_info.user_layer)
    {
        return true;
    }

    auto worker_iter = layer_info.worker_instances.find(_worker_id);
    if (worker_iter == layer_info.worker_instances.end())
    {
        // This is a complicated situation, because the layer is valid and should have the information
        // about every registered worker, but at the same time a worker could have just connected and
        // disconnected and depending on how this is managed internally (right now, this situation is
        // possible) this could be valid, for now just log a warning but return true
        Jani::MessageLog().Warning("WorldController -> Trying to handle worker disconnection but there is no worker registered with the given ID in the current layer info, worker_id {}, layer_id {}", _worker_id, _layer_id);
        return true;
    }

    auto worker_info_node = layer_info.worker_instances.extract(_worker_id);
    auto& worker_info     = worker_info_node.mapped();
    
    layer_info.ordered_worker_density_info.erase(WorkerDensityKey(worker_info.GetDensityKey()));

    WorkerInfo*                     target_worker_info = nullptr;
    std::optional<WorkerDensityKey> target_worker_density_key;
    if (layer_info.worker_instances.size() == 0)
    {
        target_worker_info = &layer_info.dummy_layer_worker_instance;
    }
    else
    {
        target_worker_info        = layer_info.ordered_worker_density_info.begin()->second;
        target_worker_density_key = layer_info.ordered_worker_density_info.begin()->first;
    }

    for (auto& cell_coordinates : worker_info.worker_cells_infos.coordinates_owned)
    {
        auto& cell_info                             = m_world_grid->AtMutable(cell_coordinates);
        WorkerDensityKey current_worker_density_key = WorkerDensityKey(*cell_info.worker_cells_infos[_layer_id]);

        MigrateEntitiesFromCellToNewWorker(
            layer_info,
            cell_info,
            worker_info,
            *target_worker_info,
            std::nullopt, 
            target_worker_density_key,
            false);

        // Update the target worker info and density key since they probably changed after this layer migration
        if (layer_info.worker_instances.size() != 0)
        {
            target_worker_info        = layer_info.ordered_worker_density_info.begin()->second;
            target_worker_density_key = layer_info.ordered_worker_density_info.begin()->first;
        }
    }

    worker_info.worker_cells_infos.coordinates_owned.clear();
    worker_info.worker_cells_infos.worker_instance = nullptr;

    return true;
}

const Jani::WorldCellInfo& Jani::RuntimeWorldController::GetWorldCellInfo(WorldCellCoordinates _coordinates) const
{
    return m_world_grid->At(_coordinates);
}

const std::unordered_map<Jani::WorkerId, Jani::RuntimeWorldController::WorkerInfo>& Jani::RuntimeWorldController::GetWorkersInfosForLayer(LayerId _layer_id) const
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

void Jani::RuntimeWorldController::ValidateLayersWithoutWorkers() const
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

void Jani::RuntimeWorldController::InsertEntity(ServerEntity& _entity, WorldPosition _position)
{
    /*
    * Ideally this function should check if there are enough workers available for each layer in order
    * to insert this entity, if that is not the case, we should add this entity into a list that will
    * be checked against when registering a worker for the missing layer
    * Maybe we should mark this entity as invalid until there are enough layers to attend to it
    *
    * Actually I needs something different, because it could be that a worker crashes, leaving a layer
    * without available workers, this should be handled somehow...
    *
    * Maybe we should use a placeholder worker when there are no workers available for a given layer and
    * send all cells used by this placeholder to the first worker created?


        1 - Fazer a verificacao ao adicionar um novo worker para um layer
            - Chamar o callback de troca de cell durante essa acao pode nao ser necessariamente uma boa ideia

        2 - Fazer a verificacao durante Update()
            

    */

    PushDebugCommand("InsertEntity");

    WorldCellCoordinates cell_coordinates = ConvertPositionIntoCellCoordinates(_position);

    SetupCell(cell_coordinates);

    auto& cell_info = m_world_grid->AtMutable(cell_coordinates);

    assert(cell_info.entities.find(_entity.GetId()) == cell_info.entities.end());

    _entity.SetWorldCellInfo(cell_info);

    // Do something about each worker that owns the given cell? (for each layer)

    cell_info.entities.insert({ _entity.GetId(), &_entity });

    SetupWorkCellEntityInsertion(cell_info);
}

void Jani::RuntimeWorldController::RemoveEntity(ServerEntity& _entity)
{
    PushDebugCommand("RemoveEntity");

    WorldCellCoordinates cell_coordinates = _entity.GetWorldCellCoordinates();
    auto&                cell_info        = m_world_grid->AtMutable(cell_coordinates);

    // Do something about each worker that owns the given cell? (for each layer)

    cell_info.entities.erase(_entity.GetId());

    SetupWorkCellEntityRemoval(cell_info);
}

void Jani::RuntimeWorldController::SetupWorkCellEntityInsertion(WorldCellInfo& _cell_info, std::optional<LayerId> _layer)
{
    PushDebugCommand("SetupWorkCellEntityInsertion");

    if (_layer)
    {
        auto& layer_info = m_layer_infos[_layer.value()];

        if (!layer_info)
        {
            return;
        }

        assert(layer_info->ordered_worker_density_info.size() > 0);

        auto& current_worker_cells_infos = _cell_info.worker_cells_infos[layer_info->layer_id];
        auto extracted_worker_node       = layer_info->ordered_worker_density_info.extract(WorkerDensityKey(*current_worker_cells_infos));
        {
            extracted_worker_node.key().global_entity_count++;
            extracted_worker_node.mapped()->worker_cells_infos.entity_count++;
            extracted_worker_node.mapped()->worker_cells_infos.coordinates_owned.insert(_cell_info.cell_coordinates);
        }
        auto insert_iter = layer_info->ordered_worker_density_info.insert(std::move(extracted_worker_node));
    }
    else
    {
        for (auto& layer_info : m_layer_infos)
        {
            if (!layer_info)
            {
                break;
            }

            assert(layer_info->ordered_worker_density_info.size() > 0);

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

}

void Jani::RuntimeWorldController::SetupWorkCellEntityRemoval(WorldCellInfo& _cell_info, std::optional<LayerId> _layer)
{
    PushDebugCommand("SetupWorkCellEntityRemoval");

    if (_layer)
    {
        auto& layer_info = m_layer_infos[_layer.value()];

        if (!layer_info)
        {
            return;
        }

        auto& current_worker_cells_infos = _cell_info.worker_cells_infos[layer_info->layer_id];
        auto extracted_worker_node       = layer_info->ordered_worker_density_info.extract(WorkerDensityKey(*current_worker_cells_infos));
        {
            extracted_worker_node.key().global_entity_count--;
            extracted_worker_node.mapped()->worker_cells_infos.entity_count--;
        }
        auto insert_iter = layer_info->ordered_worker_density_info.insert(std::move(extracted_worker_node));
    }
    else
    {
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

}

void Jani::RuntimeWorldController::MigrateEntitiesFromCellToNewWorker(
    LayerInfo&                      _layer_info, 
    WorldCellInfo&                  _cell_info,
    WorkerInfo&                     _current_worker_info,
    WorkerInfo&                     _target_worker_info, 
    std::optional<WorkerDensityKey> _current_worker_density_key,
    std::optional<WorkerDensityKey> _target_worker_density_key, 
    bool                            _erase_from_current_worker)
{
    _target_worker_info.worker_cells_infos.entity_count  += _cell_info.entities.size();
    _current_worker_info.worker_cells_infos.entity_count -= _cell_info.entities.size();

    if(_erase_from_current_worker) _current_worker_info.worker_cells_infos.coordinates_owned.erase(_cell_info.cell_coordinates);
    _target_worker_info.worker_cells_infos.coordinates_owned.insert(_cell_info.cell_coordinates);
 
    if (_current_worker_density_key)
    {
        auto over_capacity_extracted_worker_node                      = _layer_info.ordered_worker_density_info.extract(_current_worker_density_key.value());
        over_capacity_extracted_worker_node.key().global_entity_count = _current_worker_info.worker_cells_infos.entity_count;
        _layer_info.ordered_worker_density_info.insert(std::move(over_capacity_extracted_worker_node));
    }

    if (_target_worker_density_key)
    {
        auto target_extracted_worker_node                      = _layer_info.ordered_worker_density_info.extract(_target_worker_density_key.value());
        target_extracted_worker_node.key().global_entity_count = _target_worker_info.worker_cells_infos.entity_count;
        _layer_info.ordered_worker_density_info.insert(std::move(target_extracted_worker_node));
    }

    _cell_info.worker_cells_infos[_layer_info.layer_id] = &_target_worker_info.worker_cells_infos;

    assert(m_cell_ownership_change_callback);
    m_cell_ownership_change_callback(
        _cell_info.entities,
        _cell_info.cell_coordinates,
        _layer_info.layer_id,
        _current_worker_info.worker_instance.get(),
        _target_worker_info.worker_instance.get());
}

void Jani::RuntimeWorldController::AcknowledgeEntityPositionChange(ServerEntity& _entity, WorldPosition _new_position)
{
    WorldCellCoordinates current_world_cell_coordinates = _entity.GetWorldCellCoordinates();
    WorldCellCoordinates new_world_cell_coordinates     = ConvertPositionIntoCellCoordinates(_new_position); // TODO: Fill this and remember to add a little padding so entities won't be moving between workers all the time

    if (current_world_cell_coordinates == new_world_cell_coordinates)
    {
        return;
    }

    WorldPosition current_cell_world_position = ConvertCellCoordinatesIntoPosition(current_world_cell_coordinates);
    WorldPosition new_cell_world_position     = ConvertCellCoordinatesIntoPosition(new_world_cell_coordinates);

    float half_cell_unit_length    = (m_deployment_config.GetMaximumWorldLength() / m_deployment_config.GetTotalGridsPerWorldLine()) / 2.0f;
    float distance_to_current_cell = glm::distance(glm::vec2(current_cell_world_position.x + half_cell_unit_length, current_cell_world_position.y + half_cell_unit_length), glm::vec2(_new_position.x, _new_position.y));
    float distance_to_new_cell     = glm::distance(glm::vec2(new_cell_world_position.x + half_cell_unit_length, new_cell_world_position.y + half_cell_unit_length), glm::vec2(_new_position.x, _new_position.y));

    // If we are at the border, we can tolerate a little amount before performing the cell change
    if (distance_to_new_cell / distance_to_current_cell > 0.7f) // 0.0 = at center, 1.0 = at border
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

    current_world_cell_info.entities.erase(_entity.GetId());
    new_world_cell_info.entities.insert({ _entity.GetId(), &_entity });

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

        SetupWorkCellEntityInsertion(new_world_cell_info, layer_info->layer_id);
        SetupWorkCellEntityRemoval(current_world_cell_info, layer_info->layer_id);

        assert(m_entity_layer_ownership_change_callback);
        m_entity_layer_ownership_change_callback(
            _entity, 
            layer_info->layer_id, 
            *current_layer_worker, 
            *new_layer_worker);
    }
}

void Jani::RuntimeWorldController::SetupCell(WorldCellCoordinates _cell_coordinates)
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

        // If there are not workers available, use the dummy one
        if (layer_info.value().ordered_worker_density_info.size() == 0)
        {
            layer_info->dummy_layer_worker_instance.worker_cells_infos.coordinates_owned.insert(_cell_coordinates);

            // Make the cell point to the right worker cells infos
            cell_info.worker_cells_infos[i] = &layer_info->dummy_layer_worker_instance.worker_cells_infos;
        }
        // Insert normally
        else
        {
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
}

void Jani::RuntimeWorldController::RegisterCellOwnershipChangeCallback(CellOwnershipChangeCallback _callback)
{
    m_cell_ownership_change_callback = _callback;
}

void Jani::RuntimeWorldController::RegisterEntityLayerOwnershipChangeCallback(EntityLayerOwnershipChangeCallback _callback)
{
    m_entity_layer_ownership_change_callback = _callback;
}

void Jani::RuntimeWorldController::RegisterWorkerLayerRequestCallback(WorkerLayerRequestCallback _callback)
{
    m_worker_layer_request_callback = _callback;
}

Jani::WorldCellCoordinates Jani::RuntimeWorldController::ConvertPositionIntoCellCoordinates(WorldPosition _world_position) const
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

Jani::WorldPosition Jani::RuntimeWorldController::ConvertCellCoordinatesIntoPosition(WorldPosition _cell_coordinates) const
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

float Jani::RuntimeWorldController::ConvertWorldScalarIntoCellScalar(float _scalar) const
{
    auto maximum_world_length = m_deployment_config.GetMaximumWorldLength();

    assert(m_deployment_config.GetMaximumWorldLength() % m_deployment_config.GetTotalGridsPerWorldLine() == 0);

    uint32_t cell_unit_length = m_deployment_config.GetMaximumWorldLength() / m_deployment_config.GetTotalGridsPerWorldLine();

    _scalar /= static_cast<float>(cell_unit_length);

    return _scalar;
}

void Jani::RuntimeWorldController::ForEachEntityOnRadius(WorldPosition _world_position, float _radius, std::function<void(EntityId, ServerEntity&, WorldCellCoordinates)> _callback) const
{
    WorldCellCoordinates cell_coordinates = ConvertPositionIntoCellCoordinates(_world_position);
    float                cell_radius      = ConvertWorldScalarIntoCellScalar(_radius);

    auto selected_cells = m_world_grid->InsideRange(cell_coordinates, cell_radius);
    for (auto& cell : selected_cells)
    {
        for (auto& [entity_id, entity] : cell->entities)
        {
            if (glm::distance(glm::vec2(_world_position), glm::vec2(entity->GetWorldPosition())) < _radius)
            {
                _callback(entity_id, *entity, cell->cell_coordinates);
            }
        }
    }
}

void Jani::RuntimeWorldController::ForEachEntityOnRect(WorldRect _world_rect, std::function<void(EntityId, ServerEntity&, WorldCellCoordinates)> _callback) const
{
    int32_t              cell_unit_length = m_deployment_config.GetMaximumWorldLength() / m_deployment_config.GetTotalGridsPerWorldLine();
    WorldCellCoordinates rect_begin       = ConvertPositionIntoCellCoordinates(WorldPosition({ _world_rect.x, _world_rect.y }));
    WorldCellCoordinates rect_size        = WorldCellCoordinates({ _world_rect.width / cell_unit_length, _world_rect.height / cell_unit_length });
    WorldCellCoordinates rect_end         = WorldCellCoordinates({ rect_begin.x + rect_size.x, rect_begin.y + rect_size.y });

    auto selected_cells = m_world_grid->InsideRectMutable(rect_begin, rect_end);
    for (auto& cell : selected_cells)
    {
        for (auto& [entity_id, entity] : cell->entities)
        {
            WorldPosition entity_world_pos = entity->GetWorldPosition();
            if (entity_world_pos.x > _world_rect.x
                && entity_world_pos.y > _world_rect.y
                && entity_world_pos.x < _world_rect.x + _world_rect.width
                && entity_world_pos.y < _world_rect.y + _world_rect.height)
            {
                _callback(entity_id, *entity, cell->cell_coordinates);
            }
        }
    }
}

void Jani::RuntimeWorldController::ApplySpatialBalance()
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
            auto  cell_coordinates = *coordinates_iter;
            auto& cell_info        = m_world_grid->AtMutable(cell_coordinates);

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

                WorkerDensityKey over_capacity_worker_density_key = WorkerDensityKey(*cell_info.worker_cells_infos[layer_info.value().layer_id]);

                MigrateEntitiesFromCellToNewWorker(
                    layer_info.value(),
                    cell_info,
                    *worker_instance_over_limit,
                    *target_worker_info,
                    over_capacity_worker_density_key,
                    target_worker_density_key,
                    false);

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