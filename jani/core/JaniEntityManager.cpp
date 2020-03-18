////////////////////////////////////////////////////////////////////////////////
// Filename: EntityManager.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniEntityManager.h"

Jani::EntityManager::EntityManager(Worker& _worker) : m_worker(_worker)
{
#if 0
    _worker.RegisterOnComponentUpdateCallback(
        [](entityx::Entity& _entity, Jani::ComponentId _component_id, const Jani::ComponentPayload& _component_payload)
        {
            // Somehow I know that component id 0 translate to Position component
            switch (_component_id)
            {
                case 0:
                {
                    _entity.replace<PositionComponent>(_component_payload.GetPayload<PositionComponent>());

                    break;
                }
                case 1:
                {
                    _entity.replace<NpcComponent>(_component_payload.GetPayload<NpcComponent>());

                    break;
                }
                default:
                {
                    Jani::MessageLog().Error("Worker -> Trying to assign invalid component to entity");

                    break;
                }
            }
        });

    _worker.RegisterOnComponentRemovedCallback(
        [](entityx::Entity& _entity, Jani::ComponentId _component_id)
        {
            // Somehow I know that component id 0 translate to Position component
            switch (_component_id)
            {
                case 0:
                {
                    assert(_entity.has_component<PositionComponent>());
                    _entity.remove<PositionComponent>();

                    break;
                }
                case 1:
                {
                    assert(_entity.has_component<NpcComponent>());
                    _entity.remove<NpcComponent>();

                    break;
                }
                default:
                {
                    Jani::MessageLog().Error("Worker -> Trying to remove invalid component from entity");

                    break;
                }
            }
        });

    _worker.RegisterOnAuthorityGainCallback(
        [&](entityx::Entity& _entity)
        {
            if (is_brain)
            {
                return;
            }

            Jani::ComponentQuery component_query;
            auto* query_instruction = component_query
                .QueryComponent(0)
                .WithFrequency(1)
                .Begin(0);

            auto and_query = query_instruction->And();
            and_query.first->EntitiesInRadius(30.0f);
            and_query.second->RequireComponent(0);

            worker.RequestUpdateComponentInterestQuery(
                worker.GetJaniEntityId(_entity).value(),
                0,
                { std::move(component_query) });
        });

    _worker.RegisterOnAuthorityLostCallback(
        [](entityx::Entity& _entity)
        {

        });
#endif
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