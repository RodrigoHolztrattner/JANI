////////////////////////////////////////////////////////////////////////////////
// Filename: EntityManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniEntityManager.h"

Jani::EntityManager::EntityManager(Worker& _worker) : m_worker(_worker)
{
    _worker.RegisterOnComponentUpdateCallback(
        [&](EntityId _entity_id, Jani::ComponentId _component_id, const Jani::ComponentPayload& _component_payload)
        {
            // Check if this entity is registered
            auto entity_iter = m_server_id_to_local.find(_entity_id);
            if (entity_iter != m_server_id_to_local.end())
            {
                if (_component_id >= m_component_operators.size())
                {
                    MessageLog().Error("EntityManager -> Trying to update an entity component but the provided component id is out of range, component id: {}", _component_id);
                    return;
                }

                if (!m_component_operators[_component_id].has_value())
                {
                    MessageLog().Error("EntityManager -> Trying to update an entity component but the provided component id isn't registered, component id: {}", _component_id);
                    return;
                }

                if (!m_component_operators[_component_id].value().update_component_function(entity_iter->second, _component_payload))
                {
                    MessageLog().Error("EntityManager -> Trying to update an entity component but the update operation failed, component id: {}", _component_id);
                    return;
                }
            }
            else
            {
                MessageLog().Error("EntityManager -> Trying to update an entity component but entity isn't registered, entity id: {}", _entity_id);
                return;
            }
        });

    _worker.RegisterOnComponentRemoveCallback(
        [&](EntityId _entity_id, Jani::ComponentId _component_id)
        {
            // Check if this entity is registered
            auto entity_iter = m_server_id_to_local.find(_entity_id);
            if (entity_iter != m_server_id_to_local.end())
            {
                if (_component_id >= m_component_operators.size())
                {
                    MessageLog().Error("EntityManager -> Trying to remove an entity component but the provided component id is out of range, component id: {}", _component_id);
                    return;
                }

                if (!m_component_operators[_component_id].has_value())
                {
                    MessageLog().Error("EntityManager -> Trying to remove an entity component but the provided component id isn't registered, component id: {}", _component_id);
                    return;
                }

                if (!m_component_operators[_component_id].value().remove_component_function(entity_iter->second))
                {
                    MessageLog().Error("EntityManager -> Trying to remove an entity component but the remove operation failed, component id: {}", _component_id);
                    return;
                }
            }
            else
            {
                MessageLog().Error("EntityManager -> Trying to remove an entity component but entity isn't registered, entity id: {}", _entity_id);
                return;
            }
        });

    _worker.RegisterOnAuthorityGainCallback(
        [&](EntityId _entity_id)
        {

        });

    _worker.RegisterOnAuthorityLostCallback(
        [](EntityId _entity_id)
        {

        });

    _worker.RegisterOnEntityCreateCallback(
        [&](EntityId _entity_id)
        {
            // Make sure there isn't an existing entity with this id
            auto entity_iter = m_server_id_to_local.find(_entity_id);
            if (entity_iter != m_server_id_to_local.end())
            {
                MessageLog().Error("EntityManager -> OnEntityCreateCallback called but there is already a linked entity with the same id: {}", _entity_id);

                if (entity_iter->second.valid() && m_entity_infos[entity_iter->second.id().index()].has_value())
                {
                    m_entity_infos[entity_iter->second.id().index()] = std::nullopt;
                }

                entity_iter->second.destroy();

                m_server_id_to_local.erase(_entity_id);
            }

            auto new_entityx       = m_ecs_manager.entities.create();
            auto new_entityx_index = new_entityx.id().index();
            if (new_entityx_index >= m_entity_infos.size())
            {
                MessageLog().Info("EntityManager -> Expanding the maximum entity infos vector to support up to {} entities", new_entityx_index + 1);
                m_entity_infos.resize(new_entityx_index + 1);
            }

            EntityInfo new_entity_info;
            new_entity_info.entityx          = new_entityx;
            new_entity_info.server_entity_id = _entity_id;
            new_entity_info.is_pure_local    = false;

            m_entity_infos[new_entityx_index] = std::move(new_entity_info);
            m_server_id_to_local.insert({ _entity_id, new_entityx });

            //
            //
            //

            Jani::ComponentQuery component_query;
            auto* query_instruction = component_query
                .QueryComponent(0)
                .WithFrequency(1)
                .Begin(0);

            auto and_query = query_instruction->And();
            and_query.first->EntitiesInRadius(30.0f);
            and_query.second->RequireComponent(0);

            _worker.RequestUpdateComponentInterestQuery(
                _entity_id,
                0,
                { std::move(component_query) });
        });

    _worker.RegisterOnEntityDestroyCallback(
        [&](EntityId _entity_id)
        {
            // Make sure there is an existing entity with this id
            auto entity_iter = m_server_id_to_local.find(_entity_id);
            if (entity_iter == m_server_id_to_local.end())
            {
                MessageLog().Error("EntityManager -> OnEntityDestroyCallback called but there isn't any entity linked with this id: {}", _entity_id);
                return;
            }

            if (entity_iter->second.valid() && m_entity_infos[entity_iter->second.id().index()].has_value())
            {
                m_entity_infos[entity_iter->second.id().index()] = std::nullopt;
            }

            entity_iter->second.destroy();

            m_server_id_to_local.erase(_entity_id);
        });
}

Jani::EntityManager::~EntityManager() 
{
}

bool Jani::EntityManager::DestroyEntity(const Entity& _entity)
{
    auto entity_info = GetEntityInfoMutable(_entity);
    if (entity_info)
    {
        if (entity_info.value()->is_pure_local)
        {
            DestroyEntityInfo(_entity);
            return true;
        }
        else if (entity_info.value()->server_entity_id.has_value())
        {
            // Check if this entity is owned by the current worker, if that isn't true
            // it means that another worker owns it and we can only "mark" it as deleted
            if (IsEntityOwned(entity_info.value()->server_entity_id.value()))
            {
                m_worker.RequestRemoveEntity(entity_info.value()->server_entity_id.value());

                if (m_apply_before_server_acknowledge
                    && IsEntityOwned(_entity))
                {
                    DestroyEntityInfo(_entity);
                }
            }
            else
            {
                entity_info.value()->is_delete_hidden = true;
            }

            return true;
        }
    }

    return false;
}

bool Jani::EntityManager::UpdateInterestQuery(
    const Entity&                _entity,
    ComponentId                   _target_component_id,
    std::vector<ComponentQuery>&& _queries)
{
    return false;
}

bool Jani::EntityManager::ClearInterestQuery(
    const Entity& _entity,
    ComponentId   _target_component_id)
{
    return false;
}