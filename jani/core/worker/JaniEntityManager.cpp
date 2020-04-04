////////////////////////////////////////////////////////////////////////////////
// Filename: JaniEntityManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniEntityManager.h"
#include "JaniClientEntity.h"

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

            auto new_entityx = m_ecs_manager.entities.create();
            auto new_entityx_index = new_entityx.id().index();
            if (new_entityx_index >= m_entity_infos.size())
            {
                MessageLog().Trace("EntityManager -> Expanding the maximum entity infos vector to support up to {} entities", new_entityx_index + 1);
                m_entity_infos.resize(new_entityx_index + 1);
            }

            EntityInfo new_entity_info;
            new_entity_info.entityx = new_entityx;
            new_entity_info.server_entity_id = _entity_id;
            new_entity_info.is_pure_local = false;

            m_entity_infos[new_entityx_index] = std::move(new_entity_info);
            m_server_id_to_local.insert({ _entity_id, new_entityx });

            if (m_on_entity_created_callback)
            {
                m_on_entity_created_callback({ *this, new_entityx_index });
            }
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

            if (m_on_entity_destroyed_callback && entity_iter->second.valid())
            {
                m_on_entity_destroyed_callback({ *this, entity_iter->second.id().index() });
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
    const Entity&                 _entity,
    ComponentId                   _target_component_id,
    std::vector<ComponentQuery>&& _queries)
{
    assert(_target_component_id < MaximumEntityComponents);

    auto entity_info = GetEntityInfo(_entity);
    if (entity_info && entity_info.value()->server_entity_id.has_value())
    {
        m_worker.RequestUpdateComponentInterestQuery(
            entity_info.value()->server_entity_id.value(),
            _target_component_id,
            std::move(_queries));
    }

    return false;
}

bool Jani::EntityManager::ClearInterestQuery(
    const Entity& _entity,
    ComponentId   _target_component_id)
{
    return false;
}

std::optional<Jani::EntityId> Jani::EntityManager::RetrieveNextAvailableEntityId(bool _is_waiting_for_reserve_response) const
{
    constexpr uint32_t total_entities_to_reserve = 10000;

    if (m_available_entity_id_ranges.size() > 0)
    {
        if (m_available_entity_id_ranges[0].first == m_available_entity_id_ranges[0].second)
        {
            m_available_entity_id_ranges.erase(m_available_entity_id_ranges.begin());
            return RetrieveNextAvailableEntityId();
        }

        if (m_available_entity_id_ranges.size() == 1 && m_available_entity_id_ranges[0].second - m_available_entity_id_ranges[0].first == total_entities_to_reserve / 2)
        {
            MessageLog().Info("EntityManager -> Requesting entity id range reserve, the response will not be waited on");

            m_worker.RequestReserveEntityIdRange(total_entities_to_reserve).OnResponse(
                [&](const Message::RuntimeReserveEntityIdRangeResponse& _response, bool _timeout)
                {
                    if (!_timeout && _response.succeed)
                    {
                        m_available_entity_id_ranges.push_back({ _response.id_begin, _response.id_end });
                    }
                });
        }

        return m_available_entity_id_ranges[0].first++;
    }

    if (!_is_waiting_for_reserve_response)
    {
        MessageLog().Warning("EntityManager -> Requesting emergency entity id range reserve, the response will be waited on");

        m_worker.RequestReserveEntityIdRange(total_entities_to_reserve).OnResponse(
            [&](const Message::RuntimeReserveEntityIdRangeResponse& _response, bool _timeout)
            {
                if (!_timeout && _response.succeed)
                {
                    m_available_entity_id_ranges.push_back({ _response.id_begin, _response.id_end });
                }
            }).WaitResponse();

            return RetrieveNextAvailableEntityId(true);
    }

    return std::nullopt;
}

std::optional<const Jani::EntityManager::EntityInfo*> Jani::EntityManager::GetEntityInfo(const Entity& _entity) const
{
    auto entity_id = _entity.GetLocalId();
    if (entity_id && entity_id.value() < m_entity_infos.size() && m_entity_infos[entity_id.value()].has_value())
    {
        return &m_entity_infos[entity_id.value()].value();
    }

    return std::nullopt;
}

std::optional<Jani::EntityManager::EntityInfo*> Jani::EntityManager::GetEntityInfoMutable(const Entity& _entity)
{
    auto entity_id = _entity.GetLocalId();
    if (entity_id && entity_id.value() < m_entity_infos.size() && m_entity_infos[entity_id.value()].has_value())
    {
        return &m_entity_infos[entity_id.value()].value();
    }

    return std::nullopt;
}

void Jani::EntityManager::DestroyEntityInfo(const Entity& _entity)
{
    auto entity_id = _entity.GetLocalId();
    if (entity_id && entity_id.value() < m_entity_infos.size() && m_entity_infos[entity_id.value()].has_value())
    {
        m_entity_infos[entity_id.value()].value().entityx.destroy();
        m_entity_infos[entity_id.value()] = std::nullopt;
    }
}

bool Jani::EntityManager::IsEntityOwned(const Entity& _entity) const
{
    auto entity_info = GetEntityInfo(_entity);
    if (entity_info && entity_info.value()->server_entity_id.has_value())
    {
        return IsEntityOwned(entity_info.value()->server_entity_id.value());
    }

    return false;
}

bool Jani::EntityManager::IsEntityOwned(EntityId _entity_id) const
{
    return m_worker.IsEntityOwned(_entity_id);
}

bool Jani::EntityManager::IsComponentOwned(ComponentId _component_id) const
{
    return m_worker.IsComponentOwned(_component_id);
}