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

std::optional<const Jani::ServerEntity*> Jani::Database::GetEntityById(EntityId _entity_id) const
{
    return GetEntityByIdMutable(_entity_id);
}

std::optional<Jani::ServerEntity*> Jani::Database::GetEntityByIdMutable(EntityId _entity_id) const
{
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter != m_active_entities.end())
    {
        return entity_iter->second.get();
    }

    return std::nullopt;
}

const std::map< Jani::EntityId, std::unique_ptr< Jani::ServerEntity>>& Jani::Database::GetEntities() const
{
    return m_active_entities;
}

std::optional<Jani::EntityId> Jani::Database::ReserveEntityIdRange(uint32_t _total_ids)
{
    static uint64_t current_entity_count = 0;
    current_entity_count += _total_ids;
    return current_entity_count - _total_ids;
}

std::optional<Jani::ServerEntity*> Jani::Database::AddEntity(
    WorkerId             _worker_id,
    EntityId             _entity_id,
    const EntityPayload& _entity_payload)
{
    // Make sure there is not active entity with the given id
    if (m_active_entities.find(_entity_id) != m_active_entities.end())
    {
        return std::nullopt;
    }

    // Create the new entity
    auto& entity = m_active_entities.insert({ _entity_id, std::make_unique<ServerEntity>(_entity_id) }).first->second;

    // Add each component contained on the payload
    for (auto& component_payload : _entity_payload.component_payloads)
    {
        entity->AddComponent(component_payload.component_id, component_payload);
    }

    return entity.get();
}

bool Jani::Database::RemoveEntity(
    WorkerId _worker_id,
    EntityId _entity_id)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return false;
    }

    auto& entity = entity_iter->second;

    return true;
}

std::optional<Jani::ServerEntity*> Jani::Database::AddComponent(
    WorkerId                _worker_id,
    EntityId                _entity_id,
    LayerId                 _layer_id,
    ComponentId             _component_id,
    const ComponentPayload& _component_payload)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return std::nullopt;
    }

    auto& entity = entity_iter->second;

    // Check if this entity already has the given component id
    if (entity->HasComponent(_component_id))
    {
        return std::nullopt;
    }

    // Add the component to the entity
    entity->AddComponent(_component_id, _component_payload);

    return entity.get();
}

std::optional<Jani::ServerEntity*> Jani::Database::RemoveComponent(
    WorkerId    _worker_id,
    EntityId    _entity_id,
    LayerId     _layer_id,
    ComponentId _component_id)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return std::nullopt;
    }

    auto& entity = entity_iter->second;

    // Check if this entity has the given component id
    if (!entity->HasComponent(_component_id))
    {
        return std::nullopt;
    }

    // Remove the component from the entity
    entity->RemoveComponent(_component_id);

    return entity.get();
}

std::optional<Jani::ServerEntity*> Jani::Database::ComponentUpdate(
    WorkerId                     _worker_id,
    EntityId                     _entity_id,
    LayerId                      _layer_id,
    ComponentId                  _component_id,
    const ComponentPayload&      _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    // Check if this entity is active
    auto entity_iter = m_active_entities.find(_entity_id);
    if (entity_iter == m_active_entities.end())
    {
        return std::nullopt;
    }

    auto& entity = entity_iter->second;

    // Check if this entity already has the given component id
    if (!entity->HasComponent(_component_id))
    {
        return std::nullopt;
    }

    if (!entity->UpdateComponent(_component_id, _component_payload))
    {
        return std::nullopt;
    }

    return entity.get();
}