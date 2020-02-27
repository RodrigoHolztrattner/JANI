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

////////////////////////////////////////////////////////////////////////////////
// Class name: Runtime
////////////////////////////////////////////////////////////////////////////////
class Runtime
{
    friend Bridge;

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Runtime(Database& _database);
    ~Runtime();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    
    */
    bool Initialize(
        std::unique_ptr<LayerCollection>         layer_collection, 
        std::unique_ptr<WorkerSpawnerCollection> worker_spawner_collection);

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
        LayerHash                _layer_hash, 
        Connection<>::ClientHash _client_hash, 
        bool                     _is_user);

/////////////////////////////////////
protected: // WORKER COMMUNICATION //
/////////////////////////////////////

    /*
    * Received when a worker sends a log message
    */
    bool OnWorkerLogMessage(
        WorkerId       _worker_id,
        WorkerLogLevel _log_level,
        std::string    _log_title,
        std::string    _log_message);

    /*
    * Received when a worker sends a request to reserve a range of entity ids
    */
    std::optional<Jani::EntityId> OnWorkerReserveEntityIdRange(
        WorkerId _worker_id, 
        uint32_t _total_ids);

    /*
    * Received when a worker requests to add a new entity
    */
    bool OnWorkerAddEntity(
        WorkerId             _worker_id,
        EntityId             _entity_id,
        const EntityPayload& _entity_payload);

    /*
    * Received when a worker requests to remove an existing entity
    */
    bool OnWorkerRemoveEntity(
        WorkerId _worker_id, 
        EntityId _entity_id);

    /*
    * Received when a worker requests to add a new component for the given entity
    */
    bool OnWorkerAddComponent(
        WorkerId                _worker_id, 
        EntityId                _entity_id, 
        ComponentId             _component_id, 
        const ComponentPayload& _component_payload);

    /*
    * Received when a worker requests to remove an existing component for the given entity
    */
    bool OnWorkerRemoveComponent(
        WorkerId _worker_id,
        EntityId _entity_id, 
        ComponentId _component_id);

    /*
    * Received when a worker updates a component
    * Can only be received if this worker has write authority over the indicated entity
    * If this component changes the entity world position, it will generate an entity position change event over the runtime
    */
    bool OnWorkerComponentUpdate(
        WorkerId                     _worker_id, 
        EntityId                     _entity_id, 
        ComponentId                  _component_id, 
        const ComponentPayload&      _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

    /*
    * Received when a worker request a component query
    */
    WorkerRequestResult OnWorkerComponentQuery(
        WorkerId              _worker_id,
        EntityId              _entity_id,
        const ComponentQuery& _component_query);

private:

    /*
    *
    */
    void ApplyLoadBalanceUpdate();

////////////////////////
private: // VARIABLES //
////////////////////////

    Database& m_database;

    std::unique_ptr<Connection<>>              m_client_connections;
    std::unique_ptr<Connection<>>              m_worker_connections;

    std::vector<std::unique_ptr<WorkerSpawnerInstance>> m_worker_spawner_instances;

    std::unique_ptr<RequestManager> m_request_manager;

    std::unique_ptr<LayerCollection>           m_layer_collection;
    std::unique_ptr<WorkerSpawnerCollection>   m_worker_spawner_collection;

    std::map<EntityId, Entity>                                    m_active_entities;
    std::map<Hash, std::unique_ptr<Bridge>>                       m_bridges;
    std::unordered_map<Connection<>::ClientHash, WorkerInstance*> m_worker_instance_mapping;

    std::chrono::time_point<std::chrono::steady_clock> m_load_balance_previous_update_time = std::chrono::steady_clock::now();
};

// Jani
JaniNamespaceEnd(Jani)