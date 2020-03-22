////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.h
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

class RuntimeWorldController;
class RuntimeWorkerReference;

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
        RuntimeWorkerReference&      _worker_instance,
        WorkerId             _worker_id,
        EntityId             _entity_id,
        const EntityPayload& _entity_payload);

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
        RuntimeWorkerReference&         _worker_instance,
        WorkerId                _worker_id, 
        EntityId                _entity_id, 
        ComponentId             _component_id, 
        const ComponentPayload& _component_payload);

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
        RuntimeWorkerReference&              _worker_instance,
        WorkerId                     _worker_id, 
        EntityId                     _entity_id, 
        ComponentId                  _component_id, 
        const ComponentPayload&      _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

    /*
    * Received when a worker request to update a component query
    */
    bool OnWorkerComponentInterestQueryUpdate(
        RuntimeWorkerReference&                    _worker_instance,
        WorkerId                           _worker_id,
        EntityId                           _entity_id,
        ComponentId                        _component_id,
        const std::vector<ComponentQuery>& _component_queries);
    
    /*
    * Received when a worker request a component query 
    */
    std::vector<std::pair<ComponentMask, std::vector<ComponentPayload>>> OnWorkerComponentInterestQuery(
        RuntimeWorkerReference&                    _worker_instance,
        WorkerId                           _worker_id,
        EntityId                           _entity_id,
        ComponentId                        _component_id) const;

private:

    /*
    * Perform a component query, optionally it can ignore entities owned by the given worker
    */
    std::vector<std::pair<Jani::ComponentMask, std::vector<Jani::ComponentPayload>>> PerformComponentQuery(
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
};

// Jani
JaniNamespaceEnd(Jani)