////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"
#include "JaniRuntimeThreadContext.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

class RuntimeWorldController;
class RuntimeWorkerReference;

struct EntityQueryController
{
    struct QueryInfo
    {
        EntityId     entity_id     = std::numeric_limits<EntityId>::max();
        ComponentId  component_id  = std::numeric_limits<ComponentId>::max();
        uint32_t     query_version = std::numeric_limits<uint32_t>::max();
        mutable bool is_outdated   = false;
    };

    void InsertQuery(EntityId _entity_id, ComponentId _component_id, QueryUpdateFrequency _update_frequency, uint32_t _query_version)
    {
        /*
        if (magic_enum::enum_integer(_update_frequency) > magic_enum::enum_integer(QueryUpdateFrequency::Max)
            || !magic_enum::enum_index(_update_frequency).has_value())
        {
            return;
        }
        */

        auto GetListIndexForFrequencyEnum = [](QueryUpdateFrequency _frequency_enum) -> uint32_t
        {
            switch (_frequency_enum)
            {
            case QueryUpdateFrequency::_50: return 0;
            case QueryUpdateFrequency::_40: return 1;
            case QueryUpdateFrequency::_30: return 2;
            case QueryUpdateFrequency::_20: return 3;
            case QueryUpdateFrequency::_10: return 4;
            case QueryUpdateFrequency::_5: return 5;
            default: return 6;
            }
        };

        QueryInfo new_entry;
        new_entry.entity_id     = _entity_id;
        new_entry.component_id  = _component_id;
        new_entry.query_version = _query_version;

        uint32_t list_entry_index = GetListIndexForFrequencyEnum(_update_frequency);
        auto&     free_list       = free_lists[list_entry_index];
        safety.lock();
        if (free_list.size() > 0)
        {
            auto infos_index = free_list.back();
            free_list.pop_back();
            safety.unlock();

            query_infos[list_entry_index][infos_index] = std::move(new_entry);
        }
        else
        {
            query_infos[list_entry_index].push_back(std::move(new_entry));
            safety.unlock();
        }
    }

    void ForEach(std::function<void(QueryInfo&)> _callback)
    {
        auto time_now         = std::chrono::steady_clock::now();
        uint64_t elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - initial_timestamp).count();

        auto GetFrequencyFromEnum = [](QueryUpdateFrequency _frequency_enum) -> uint32_t
        {
            switch (static_cast<uint32_t>(_frequency_enum))
            {
                case 0: return 50;
                case 1: return 40;
                case 2: return 30;
                case 3: return 20;
                case 4: return 10;
                case 5: return 5;
                default: return 1;
            }
        };

        for (int i = 0; i < query_infos.size(); i++)
        {
            uint32_t required_elapsed_time = 1000 / GetFrequencyFromEnum(static_cast<QueryUpdateFrequency>(i));

            if (elapsed_time / required_elapsed_time <= previous_elapsed_time / required_elapsed_time)
            {
                continue;
            }

            for (int info_index = 0; info_index < query_infos[i].size(); info_index++)
            {
                if (query_infos[i][info_index])
                {
                    if (query_infos[i][info_index]->is_outdated)
                    {
                        auto& free_list = free_lists[i];
                        {
                            std::lock_guard l(safety);
                            free_list.push_back(info_index);
                        }
                        query_infos[i][info_index] = std::nullopt;
                    }
                    else
                    {
                        _callback(*query_infos[i][info_index]);
                    }
                }
            }
        }

        previous_elapsed_time = elapsed_time;
    }


    std::array<std::vector<std::optional<QueryInfo>>, magic_enum::enum_integer(QueryUpdateFrequency::Count)> query_infos;
    std::array<std::vector<uint32_t>, magic_enum::enum_integer(QueryUpdateFrequency::Count)>                 free_lists;
    std::chrono::time_point<std::chrono::steady_clock>                                                       initial_timestamp     = std::chrono::steady_clock::now();
    uint64_t                                                                                                 previous_elapsed_time = 0;
    std::mutex                                                                                               safety;

#if 0

    /*
    *
    * Linked list implementation
    *
    */

    struct QueryInfo
    {
        EntityId                   entity_id    = std::numeric_limits<EntityId>::max();
        ComponentId                component_id = std::numeric_limits<ComponentId>::max();

        std::unique_ptr<QueryInfo> next;
        QueryInfo*                 previous     = nullptr;
    };

    void InsertQuery(EntityId _entity_id, ComponentId _component_id, QueryUpdateFrequency _update_frequency)
    {
        if (magic_enum::enum_integer(_update_frequency) > magic_enum::enum_integer(QueryUpdateFrequency::Max)
            || !magic_enum::enum_index(_update_frequency).has_value())
        {
            return;
        }

        std::unique_ptr<QueryInfo> new_query_info;
        if (free_nodes.size() > 0)
        {
            new_query_info = std::move(free_nodes.back());
            free_nodes.pop_back();
        }
        else
        {
            new_query_info = std::make_unique<QueryInfo>();
        }

        new_query_info->entity_id    = _entity_id;
        new_query_info->component_id = _component_id;
        new_query_info->previous     = nullptr;

        uint32_t list_entry_index = magic_enum::enum_index(_update_frequency).value();

        if (query_infos[list_entry_index])
        {
            new_query_info->next = std::move(query_infos[list_entry_index]);
            new_query_info->next->previous = new_query_info.get();
        }

        query_infos[list_entry_index] = std::move(new_query_info);
    }

    void ForEach(std::function<bool(EntityId, ComponentId)> _callback)
    {
        auto time_now         = std::chrono::steady_clock::now();
        uint64_t elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - initial_timestamp).count();

        for (int i = 0; i < query_infos.size(); i++)
        {
            if (query_infos[i] == nullptr)
            {
                continue;
            }

            uint32_t frequency = magic_enum::enum_integer(magic_enum::enum_value<QueryUpdateFrequency>(i));

            if (elapsed_time / frequency > previous_elapsed_time / frequency)
            {
                QueryInfo* current_node = query_infos[i].get();
                while (current_node != nullptr)
                {
                    bool result = _callback(current_node->entity_id, current_node->component_id);
                    if (!result)
                    {
                        // Root
                        if (current_node->previous == nullptr)
                        {
                            auto temp = std::move(query_infos[i]);
                            query_infos[i] = std::move(temp->next);
                            current_node = nullptr;
                            if (query_infos[i])
                            {
                                query_infos[i]->previous = nullptr;
                                current_node = query_infos[i].get();
                            }

                            free_nodes.push_back(std::move(temp));
                        }
                        else
                        {
                            auto temp = std::move(current_node->previous->next);
                            current_node->previous->next = std::move(temp->next);
                            current_node = nullptr;
                            if (temp->next)
                            {
                                temp->next->previous = temp->previous;
                                current_node = temp->next.get();
                            }

                            free_nodes.push_back(std::move(temp));
                        }
                    }
                    else
                    {
                        current_node = current_node->next ? current_node->next.get() : nullptr;
                    }
                }
            }
        }

        previous_elapsed_time = elapsed_time;
    }


    std::array<std::unique_ptr<QueryInfo>, magic_enum::enum_integer(QueryUpdateFrequency::Count)> query_infos;
    std::vector<std::unique_ptr<QueryInfo>>                                                       free_nodes;
    std::chrono::time_point<std::chrono::steady_clock>                                            initial_timestamp     = std::chrono::steady_clock::now();
    uint64_t                                                                                      previous_elapsed_time = 0;

#endif
};

////////////////////////////////////////////////////////////////////////////////
// Class name: Runtime
////////////////////////////////////////////////////////////////////////////////
class Runtime
{
    friend RuntimeBridge;

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Runtime(
        RuntimeDatabase&           _database, 
        const DeploymentConfig&    _deployment_config, 
        const LayerConfig&         _layer_config,
        const WorkerSpawnerConfig& _worker_spawner_config);
    ~Runtime();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    
    */
    bool Initialize();

    /*
    *
    */
    void Update();

private:

    /*
    * Attempt to allocate a new worker for the given layer
    * This operation is susceptible to the feasibility of creating a new one according
    * to the limits imposed by the layer itself
    * If no bridge exist for the given layer, one will be attempted to be created
    */
    bool TryAllocateNewWorker(
        LayerId                  _layer_id,
        Connection<>::ClientHash _client_hash, 
        bool                     _is_user);

/////////////////////////////////////
protected: // WORKER COMMUNICATION //
/////////////////////////////////////

    /*
    * Received when a worker sends a log message
    */
    bool OnWorkerLogMessage(
        RuntimeWorkerReference& _worker_instance,
        WorkerId        _worker_id,
        WorkerLogLevel  _log_level,
        std::string     _log_title,
        std::string     _log_message);

    /*
    * Received when a worker sends a request to reserve a range of entity ids
    */
    std::optional<Jani::EntityId> OnWorkerReserveEntityIdRange(
        RuntimeWorkerReference& _worker_instance,
        WorkerId        _worker_id, 
        uint32_t        _total_ids);

    /*
    * Received when a worker requests to add a new entity
    */
    bool OnWorkerAddEntity(
        RuntimeWorkerReference& _worker_instance,
        WorkerId                _worker_id,
        EntityId                _entity_id,
        EntityPayload           _entity_payload);

    /*
    * Received when a worker requests to remove an existing entity
    */
    bool OnWorkerRemoveEntity(
        RuntimeWorkerReference& _worker_instance,
        WorkerId        _worker_id, 
        EntityId        _entity_id);

    /*
    * Received when a worker requests to add a new component for the given entity
    */
    bool OnWorkerAddComponent(
        RuntimeWorkerReference& _worker_instance,
        WorkerId                _worker_id, 
        EntityId                _entity_id, 
        ComponentId             _component_id, 
        ComponentPayload        _component_payload);

    /*
    * Received when a worker requests to remove an existing component for the given entity
    */
    bool OnWorkerRemoveComponent(
        RuntimeWorkerReference& _worker_instance,
        WorkerId        _worker_id,
        EntityId        _entity_id, 
        ComponentId     _component_id);

    /*
    * Received when a worker updates a component
    * Can only be received if this worker has write authority over the indicated entity
    * If this component changes the entity world position, it will generate an entity position change event over the runtime
    */
    bool OnWorkerComponentUpdate(
        RuntimeWorkerReference&      _worker_instance,
        WorkerId                     _worker_id, 
        EntityId                     _entity_id, 
        ComponentId                  _component_id, 
        ComponentPayload             _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

    /*
    * Received when a worker request to update a component query
    */
    bool OnWorkerComponentInterestQueryUpdate(
        RuntimeWorkerReference&     _worker_instance,
        WorkerId                    _worker_id,
        EntityId                    _entity_id,
        ComponentId                 _component_id,
        std::vector<ComponentQuery> _component_queries);
    
private:

    /*
    * Perform a component query, optionally it can ignore entities owned by the given worker
    */
    nonstd::transient_vector<std::pair<Jani::ComponentMask, nonstd::transient_vector<const Jani::ComponentPayload*>>> PerformComponentQuery(
        const ComponentQuery&   _query, 
        WorldPosition           _search_center_location, 
        std::optional<WorkerId> _ignore_worker = std::nullopt) const;

    /*
    * Perform a quick check if there is an active worker layer that accepts the
    * given component
    */
    bool IsLayerForComponentAvailable(ComponentId _component_id) const;

////////////////////////
private: // VARIABLES //
////////////////////////

    RuntimeDatabase& m_database;

    std::unique_ptr<Connection<>> m_client_connections;
    std::unique_ptr<Connection<>> m_worker_connections;
    std::unique_ptr<Connection<>> m_inspector_connections;

    std::vector<std::unique_ptr<RuntimeWorkerSpawnerReference>> m_worker_spawner_instances;

    std::unique_ptr<RequestManager> m_request_manager;

    const DeploymentConfig&    m_deployment_config;
    const LayerConfig&         m_layer_config;
    const WorkerSpawnerConfig& m_worker_spawner_config;

    std::unique_ptr<RuntimeWorldController> m_world_controller;

    std::map<LayerId, std::unique_ptr<RuntimeBridge>>                     m_bridges;
    std::unordered_map<Connection<>::ClientHash, RuntimeWorkerReference*> m_worker_instance_mapping;

    EntityQueryController m_entity_query_controller;
};

// Jani
JaniNamespaceEnd(Jani)