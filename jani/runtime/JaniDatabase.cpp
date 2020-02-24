////////////////////////////////////////////////////////////////////////////////
// Filename: JaniDatabase.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniDatabase.h"

Jani::Database::Database()
{
}

Jani::Database::~Database()
{
}

std::optional<Jani::EntityId> Jani::Database::ReserveEntityIdRange(uint32_t _total_ids)
{
    static uint64_t current_entity_count = 0;
    current_entity_count += _total_ids;
    return current_entity_count - _total_ids;
}

bool Jani::Database::AddEntity(
    EntityId             _entity_id,
    const EntityPayload& _entity_payload)
{
    return true;
}

bool Jani::Database::RemoveEntity(EntityId _entity_id)
{
    return true;
}

bool Jani::Database::AddComponent(
    EntityId                _entity_id,
    ComponentId             _component_id,
    const ComponentPayload& _component_payload)
{
    return true;
}

bool Jani::Database::RemoveComponent(
    EntityId _entity_id,
    ComponentId _component_id)
{
    return true;
}

bool Jani::Database::ComponentUpdate(
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    const ComponentPayload& _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    return true;
}