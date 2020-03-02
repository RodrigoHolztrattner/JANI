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

////////////////////////////////////////////////////////////////////////////////
// Class name: Database
////////////////////////////////////////////////////////////////////////////////
class Database
{
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Database();
    ~Database();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    // Return an entity by its identifier
    std::optional<const Entity*> GetEntityById(EntityId _entity_id) const;

    /*
    * Return the entity map
    */
    const std::map<EntityId, std::unique_ptr<Entity>>& GetEntities() const;

public:

    /*
    */
    std::optional<EntityId> ReserveEntityIdRange(uint32_t _total_ids);

    /*
    */
    std::optional<Entity*> AddEntity(
        WorkerId             _worker_id,
        EntityId             _entity_id,
        const EntityPayload& _entity_payload);

    /*
    */
    bool RemoveEntity(
        WorkerId _worker_id,
        EntityId _entity_id);

    /*
    */
    std::optional<Entity*> AddComponent(
        WorkerId                _worker_id,
        EntityId                _entity_id, 
        LayerId                 _layer_id,
        ComponentId             _component_id, 
        const ComponentPayload& _component_payload);

    /*
    */
    std::optional<Entity*> RemoveComponent(
        WorkerId    _worker_id,
        EntityId    _entity_id, 
        LayerId     _layer_id,
        ComponentId _component_id);

    /*
    */
    std::optional<Entity*> ComponentUpdate(
        WorkerId                     _worker_id,
        EntityId                     _entity_id, 
        LayerId                      _layer_id,
        ComponentId                  _component_id, 
        const ComponentPayload&      _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

////////////////////////
private: // VARIABLES //
////////////////////////

    std::map<EntityId, std::unique_ptr<Entity>> m_active_entities;

};

// Jani
JaniNamespaceEnd(Jani)