////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.h
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
// Class name: LoadBalancer
////////////////////////////////////////////////////////////////////////////////
class LoadBalancer
{
    struct WorkerLayerInfo
    {
        bool operator() (const WorkerLayerInfo& rhs) const
        {
            if (entity_count < rhs.entity_count) return true;
            if (entity_count > rhs.entity_count) return false;
            if (worker_id < rhs.worker_id) return true;

            return false;
        }

        WorkerInstance* worker_instance = nullptr;
        uint32_t        entity_count    = 0;
        WorkerId        worker_id       = std::numeric_limits<WorkerId>::max();
    };

    struct CellInfo
    {
        bool operator() (const CellInfo& rhs) const
        {
            if (entity_count < rhs.entity_count) return true;
            if (entity_count > rhs.entity_count) return false;
            if (position < rhs.position) return true;

            return false;
        }

        WorldPosition position;
        uint32_t      entity_count = 0;
    };

    struct WorkerInfo
    {
        std::unique_ptr<WorkerInstance> worker_instance;
        std::set<CellInfo>              ordered_cell_usage_info;
        uint32_t                        entity_count = 0;
    };

    struct GridCellUsageInfo
    {
        std::optional<WorldPosition> cell_position; 
        WorkerInstance*              worker_instance = nullptr;
    };

    /*
    *   - It is possible for a layer that uses spatial area to have workers that are not assigned to any
    *   grid cell, this can happen if there are no entities in the world or if a worker lost control over
    *   a cell
    */

    struct LayerInfo
    {
        bool                                                          user_layer                   = false;
        bool                                                          uses_spatial_area            = false;
        uint32_t                                                      maximum_entities_per_worker  = std::numeric_limits<uint32_t>::max();
        uint32_t                                                      maximum_workers              = std::numeric_limits<uint32_t>::max();
        std::unordered_map<WorldPosition, WorkerLayerInfo>            worker_grids;
        std::unordered_map<WorkerId, WorkerInfo>                      worker_instances;
        LayerId                                                       layer_id                     = std::numeric_limits<LayerId>::max();
        std::set<WorkerLayerInfo>                                     ordered_grid_usage_info;
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    LoadBalancer(
        const DeploymentConfig& _deployment_config,
        const LayerCollection&  _layer_collection);
    ~LoadBalancer();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    
    */
    bool Initialize();

    /*
    
    */
    void Update()
    {
        /*
            - Preciso ver se tem algum worker que ta overcapacity
            - Se tiver eu devo fazer o seguinte:
                - Pegar as cells que o worker ta usando em ordem crescente
                    - Se for so uma nao tem o que fazer...
                - Comecando da menos ocupada, tentar doar ela pra outro worker
                    - Caso nao tenha outro worker, solicitar que um novo seja criado
                - Repetir isso ate que ele nao fique mais over-capacity
                    - Na verdade fazer isso uma certa quantidade x de vezes pra nao enviar muitas mensagens
                    nesse update cicle
        */

        std::array<bool, MaximumLayers> layers_that_requires_new_workers = {}; // TODO: Move this to the class and pass it to the runtime
        // whenever applicable

        // Apply spatial balance for each layer that uses spatial area
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
                && layer_info.worker_instances.begin()->second.entity_count > layer_info.maximum_entities_per_worker
                && layer_info.maximum_workers > 1)
            {
                // Flag that another worker needs to be created for this layer
                layers_that_requires_new_workers[layer_info.layer_id] = true;

                continue;
            }

            // Determine if there is a worker instance that needs its surplus to be distributed over other
            // workers
            WorkerInfo* worker_instance_over_limit = nullptr;
            for (auto& [worker_id, worker_instance] : layer_info.worker_instances)
            {
                if (worker_instance.entity_count >= layer_info.maximum_entities_per_worker)
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
                if (cannot_continue || worker_instance_over_limit->entity_count < layer_info.maximum_entities_per_worker)
                {
                    break;
                }

                auto& cell_info = *cell_info_iter;

                bool cell_has_switched_ownership = false;
                for (auto& worker_layer_info : layer_info.ordered_grid_usage_info)
                {
                    // Do not try to move to itself
                    if (worker_layer_info.worker_id == worker_instance_over_limit->worker_instance->GetId())
                    {
                        continue;
                    }

                    // Make sure this worker is not over capacity and has enough space
                    if (worker_layer_info.entity_count + cell_info.entity_count > layer_info.maximum_entities_per_worker)
                    {
                        // There is no point trying anymore, since the `ordered_grid_usage_info` is ordered by entity size
                        // this is guaranteed to be the worker with the lowest number of entities, if it failed it means
                        // no other worker will be able to accept the entities
                        // Basically we need to try to create another worker for this layer
                        cannot_continue = true;
                        break;
                    }

                    // Move the entity ownership (caution with that function to not remove the entry on the worker instance info, 
                    // it should be done below (to not invalidate the iterator) since we are inside a loop)
                    // ...

                    cell_has_switched_ownership = true;
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
                layers_that_requires_new_workers[layer_info.layer_id] = true;
            }
        }

        // Apply spatial balance for each layer that doesn't use spatial area
        for (auto& layer_info : m_layer_infos)
        {
            if (layer_info.uses_spatial_area
                || layer_info.worker_instances.size() == 0
                || (layer_info.worker_instances.size() == 1 && layer_info.maximum_workers == 1)) // If there is one instance and the limit is
                // one, no balance is required/possible
            {
                continue;
            }

            // TODO:
            // ...
        }
    }

    /*
    * Search the input set for a bridge that can accept a new entity on the selected layer
    * This search will take in consideration the layer load balance strategies matched
    * against the entity values
    * Returning a nullopt has 2 meanings: Or there is no valid bridge for the specified
    * layer or there are but they do not match the load balance requirements for this 
    * layer, in both occasions a new bridge should be created
    * It's important to mention that, if a layer has a limit on the number of workers
    * and this limit was already achieved, any other load balancing rules will be 
    * ignored since it's impossible to create another worker, in this case the return
    * value will be the "best" possible given the case
    */
    std::optional<Bridge*> TryFindValidBridgeForLayer(
        const std::map<Hash, std::unique_ptr<Bridge>>& _bridges, 
        const Entity&                                  _triggering_entity,
        Hash                                           _layer_name_hash) const;

    /*
    * This function will return a vector with all layers that should have at least one worker
    * but are empty
    * The intention here is to call this function on startup to make sure there is at least one
    * worker available of each used layer
    * This function will ignore layers that have the maximum number of workers set to 0
    */
    std::vector<LayerId> GetLayersWithoutWorkers() const
    {
        std::vector<LayerId> missing_workers_for_layers; // No reserve is required because this should
        // only return values until there is one worker for each layer, this should return nothing 
        // during operation time
        for (auto& layer_info : m_layer_infos)
        {
            if (layer_info.worker_instances.size() == 0 && layer_info.maximum_workers > 0)
            {
                missing_workers_for_layers.push_back(layer_info.layer_id);
            }
        }

        return std::move(missing_workers_for_layers);
    }

private:

    const DeploymentConfig& m_deployment_config;
    const LayerCollection&  m_layer_collection;

    std::array<LayerInfo, MaximumLayers> m_layer_infos;
};

// Jani
JaniNamespaceEnd(Jani)