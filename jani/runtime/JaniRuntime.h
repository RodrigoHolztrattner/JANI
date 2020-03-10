////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

class WorkerInstance;
class LayerCollection;
class WorkerSpawnerCollection;
class DeploymentConfig;
class WorldController;

////////////////////////////////////////////////////////////////////////////////
// Class name: Runtime
////////////////////////////////////////////////////////////////////////////////
class Runtime
{
    friend Bridge;

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Runtime(
        Database&                      _database, 
        const DeploymentConfig&        _deployment_config, 
        const LayerCollection&         _layer_collection,
        const WorkerSpawnerCollection& _worker_spawner_collection);
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
        WorkerInstance& _worker_instance,
        WorkerId        _worker_id,
        WorkerLogLevel  _log_level,
        std::string     _log_title,
        std::string     _log_message);

    /*
    * Received when a worker sends a request to reserve a range of entity ids
    */
    std::optional<Jani::EntityId> OnWorkerReserveEntityIdRange(
        WorkerInstance& _worker_instance,
        WorkerId        _worker_id, 
        uint32_t        _total_ids);

    /*
    * Received when a worker requests to add a new entity
    */
    bool OnWorkerAddEntity(
        WorkerInstance&      _worker_instance,
        WorkerId             _worker_id,
        EntityId             _entity_id,
        const EntityPayload& _entity_payload);

    /*
    * Received when a worker requests to remove an existing entity
    */
    bool OnWorkerRemoveEntity(
        WorkerInstance& _worker_instance,
        WorkerId        _worker_id, 
        EntityId        _entity_id);

    /*
    * Received when a worker requests to add a new component for the given entity
    */
    bool OnWorkerAddComponent(
        WorkerInstance&         _worker_instance,
        WorkerId                _worker_id, 
        EntityId                _entity_id, 
        ComponentId             _component_id, 
        const ComponentPayload& _component_payload);

    /*
    * Received when a worker requests to remove an existing component for the given entity
    */
    bool OnWorkerRemoveComponent(
        WorkerInstance& _worker_instance,
        WorkerId        _worker_id,
        EntityId        _entity_id, 
        ComponentId     _component_id);

    /*
    * Received when a worker updates a component
    * Can only be received if this worker has write authority over the indicated entity
    * If this component changes the entity world position, it will generate an entity position change event over the runtime
    */
    bool OnWorkerComponentUpdate(
        WorkerInstance&              _worker_instance,
        WorkerId                     _worker_id, 
        EntityId                     _entity_id, 
        ComponentId                  _component_id, 
        const ComponentPayload&      _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

    /*
    * Received when a worker request to update a component query
    */
    bool OnWorkerComponentInterestQueryUpdate(
        WorkerInstance&                    _worker_instance,
        WorkerId                           _worker_id,
        EntityId                           _entity_id,
        ComponentId                        _component_id,
        const std::vector<ComponentQuery>& _component_queries);
    
    /*
    * Received when a worker request a component query 
    */
    std::vector<ComponentPayload> OnWorkerComponentInterestQuery(
        WorkerInstance&                    _worker_instance,
        WorkerId                           _worker_id,
        EntityId                           _entity_id,
        ComponentId                        _component_id);

private:

    /*
    * Perform a quick check if there is an active worker layer that accepts the
    * given component
    */
    bool IsLayerForComponentAvailable(ComponentId _component_id) const;

////////////////////////
private: // VARIABLES //
////////////////////////

    Database& m_database;

    std::unique_ptr<Connection<>> m_client_connections;
    std::unique_ptr<Connection<>> m_worker_connections;
    std::unique_ptr<Connection<>> m_inspector_connections;

    std::vector<std::unique_ptr<WorkerSpawnerInstance>> m_worker_spawner_instances;

    std::unique_ptr<RequestManager> m_request_manager;

    const DeploymentConfig&        m_deployment_config;
    const LayerCollection&         m_layer_collection;
    const WorkerSpawnerCollection& m_worker_spawner_collection;

    std::unique_ptr<WorldController> m_world_controller;

    std::map<LayerId, std::unique_ptr<Bridge>>                    m_bridges;
    std::unordered_map<Connection<>::ClientHash, WorkerInstance*> m_worker_instance_mapping;
};

// Jani
JaniNamespaceEnd(Jani)