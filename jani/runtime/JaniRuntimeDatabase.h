////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntimeDatabase.h
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

////////////////////////////////////////////////////////////////////////////////
// Class name: RuntimeDatabase
////////////////////////////////////////////////////////////////////////////////
class RuntimeDatabase
{
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    RuntimeDatabase();
    ~RuntimeDatabase();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    // Return an entity by its identifier
    std::optional<const ServerEntity*> GetEntityById(EntityId _entity_id) const;
    std::optional<ServerEntity*> GetEntityByIdMutable(EntityId _entity_id) const;

    /*
    * Return the entity map
    */
    const std::map<EntityId, std::unique_ptr<ServerEntity>>& GetEntities() const;

public:

    /*
    */
    std::optional<EntityId> ReserveEntityIdRange(uint32_t _total_ids);

    /*
    */
    std::optional<ServerEntity*> AddEntity(
        WorkerId      _worker_id,
        EntityId      _entity_id,
        EntityPayload _entity_payload);

    /*
    */
    bool RemoveEntity(
        WorkerId _worker_id,
        EntityId _entity_id);

    /*
    */
    std::optional<ServerEntity*> AddComponent(
        WorkerId         _worker_id,
        EntityId         _entity_id, 
        LayerId          _layer_id,
        ComponentId      _component_id, 
        ComponentPayload _component_payload);

    /*
    */
    std::optional<ServerEntity*> RemoveComponent(
        WorkerId    _worker_id,
        EntityId    _entity_id, 
        LayerId     _layer_id,
        ComponentId _component_id);

    /*
    */
    std::optional<ServerEntity*> ComponentUpdate(
        WorkerId                     _worker_id,
        EntityId                     _entity_id, 
        LayerId                      _layer_id,
        ComponentId                  _component_id, 
        ComponentPayload             _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

////////////////////////
private: // VARIABLES //
////////////////////////

    std::map<EntityId, std::unique_ptr<ServerEntity>> m_active_entities;

};

// Jani
JaniNamespaceEnd(Jani)