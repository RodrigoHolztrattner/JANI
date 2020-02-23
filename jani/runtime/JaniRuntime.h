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

class Database;
class Bridge;
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

protected:

    /*
    * Received when the underlying worker send a log message
    */
    WorkerRequestResult OnWorkerLogMessage(
        WorkerId       _worker_id,
        WorkerLogLevel _log_level,
        std::string    _log_title,
        std::string    _log_message);

    /*
    * Received when the underlying worker send a request to reserve a range of entity ids
    */
    WorkerRequestResult OnWorkerReserveEntityIdRange(
        WorkerId _worker_id, 
        uint32_t _total_ids);

    /*
    * Received when the underlying worker requests to add a new entity
    */
    WorkerRequestResult OnWorkerAddEntity(
        WorkerId             _worker_id,
        EntityId             _entity_id,
        const EntityPayload& _entity_payload);

    /*
    * Received when the underlying worker requests to remove an existing entity
    */
    WorkerRequestResult OnWorkerRemoveEntity(
        WorkerId _worker_id, 
        EntityId _entity_id);

    /*
    * Received when the underlying worker requests to add a new component for the given entity
    */
    WorkerRequestResult OnWorkerAddComponent(
        WorkerId                _worker_id, 
        EntityId                _entity_id, 
        ComponentId             _component_id, 
        const ComponentPayload& _component_payload);

    /*
    * Received when the underlying worker requests to remove an existing component for the given entity
    */
    WorkerRequestResult OnWorkerRemoveComponent(
        WorkerId _worker_id,
        EntityId _entity_id, 
        ComponentId _component_id);

    /*
    * Received when the underlying worker updates a component
    * Can only be received if this worker has write authority over the indicated entity
    * If this component changes the entity world position, it will generate an entity position change event over the runtime
    */
    WorkerRequestResult OnWorkerComponentUpdate(
        WorkerId                     _worker_id, 
        EntityId                     _entity_id, 
        ComponentId                  _component_id, 
        const ComponentPayload&      _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

private:

    /*
    *
    */
    void ReceiveIncommingUserConnections();

    /*
    *
    */
    void ApplyLoadBalanceRequirements();

////////////////////////
private: // VARIABLES //
////////////////////////

    Database& m_database;

    std::unique_ptr<LayerCollection>         m_layer_collection;
    std::unique_ptr<WorkerSpawnerCollection> m_worker_spawner_collection;

    std::map<EntityId, Entity> m_active_entities;

    std::map<Hash, std::unique_ptr<Bridge>> m_bridges;
};

// Jani
JaniNamespaceEnd(Jani)