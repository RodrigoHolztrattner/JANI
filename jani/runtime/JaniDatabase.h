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

    /*
    */
    std::optional<EntityId> ReserveEntityIdRange(uint32_t _total_ids);

    /*
    */
    bool AddEntity(
        EntityId             _entity_id,
        const EntityPayload& _entity_payload);

    /*
    */
    bool RemoveEntity(EntityId _entity_id);

    /*
    */
    bool AddComponent(
        EntityId                _entity_id, 
        ComponentId             _component_id, 
        const ComponentPayload& _component_payload);

    /*
    */
    bool RemoveComponent(
        EntityId _entity_id, 
        ComponentId _component_id);

    /*
    */
    bool ComponentUpdate(
        EntityId                     _entity_id, 
        ComponentId                  _component_id, 
        const ComponentPayload&      _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

////////////////////////
private: // VARIABLES //
////////////////////////

};

// Jani
JaniNamespaceEnd(Jani)