////////////////////////////////////////////////////////////////////////////////
// Filename: EntityManager.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"
#include "JaniWorker.h" // Necessary since we use templated functions

///////////////
// NAMESPACE //
///////////////

#define DeclareTemplateIntrospectionMethod(Method) \
template<typename T> \
struct has_##Method##_method \
{ \
private: \
    typedef std::true_type yes; \
    typedef std::false_type no; \
 \
    template<typename U> static auto test(int) -> decltype(sizeof(&std::declval<T>().##Method) > 0, yes()); \
 \
    template<typename> static no test(...); \
 \
public: \
 \
    static constexpr bool value = std::is_same<decltype(test<T>(0)), yes>::value; \
};

#define TemplateMethodExistent(T, Method) typename std::enable_if<has_##Method##_method<ComponentClass>::value>::type* = nullptr
#define TemplateMethodInexistent(T, Method) typename std::enable_if<!has_##Method##_method<ComponentClass>::value>::type* = nullptr

DeclareTemplateIntrospectionMethod(serialize);

// Jani
JaniNamespaceBegin(Jani)

////////////////////////////////////////////////////////////////////////////////
// Class name: EntityManager
////////////////////////////////////////////////////////////////////////////////
class EntityManager
{
private:

    struct EntityInfo
    {
        std::optional<EntityId> server_entity_id; // Not having this means it's a local entity (not synchronized)
        entityx::Entity         entityx;
        bool                    is_pure_local       = false;
        bool                    is_pending_deletion = false; // If it will be deleted after the system update frame
        bool                    is_delete_hidden    = false; // True if the entity was supposed to be deleted but its not owned to complete the request
    };

    struct ComponentOperators
    {
        std::function<void(entityx::Entity& _entity, ComponentPayload& _component_payload)> update_component_function;
        std::function<void(entityx::Entity& _entity)>                                       remove_component_function;
        std::function<void(entityx::Entity& _entity, ComponentPayload& _component_payload)> serialize_component_function;
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    EntityManager(Worker& _worker);
    ~EntityManager();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    template <typename ComponentClass,
        TemplateMethodExistent(ComponentClass, serialize)>
    bool RegisterComponent(ComponentId _component_id)
    {
        if (_component_id >= MaximumEntityComponents)
        {
            MessageLog().Error("EntityManager -> Trying to register a component but the id provided is out of range: {}", _component_id);
            return false;
        }

        JaniWarnOnFail(m_component_id_to_hash[_component_id] == std::nullopt && m_component_operators[_component_id] == std::nullopt, "EntityManager -> Registering component that was already registered, id: {}", _component_id);

        ComponentOperators component_operators;

        component_operators.update_component_function = 
            [](entityx::Entity& _entity, ComponentPayload& _component_payload)
        {

        };

        component_operators.remove_component_function =
            [](entityx::Entity& _entity)
        {
            _entity.remove<ComponentClass>();
        };

        component_operators.serialize_component_function =
            [](entityx::Entity& _entity, ComponentPayload& _component_payload)
        {
            _entity.remove<ComponentClass>();
        };

        m_component_id_to_hash[_component_id]  = ctti::detailed_nameof<ComponentClass>().name().hash();
        MaximumEntityComponents[_component_id] = std::move(component_operators);

        return true;
    }

    template <typename ComponentClass,
        TemplateMethodInexistent(ComponentClass, serialize)>
        bool RegisterComponent(ComponentId _component_id)
    {
        if (_component_id >= MaximumEntityComponents)
        {
            MessageLog().Error("EntityManager -> Trying to register a component but the id provided is out of range: {}", _component_id);
            return false;
        }

        JaniWarnOnFail(m_component_id_to_hash[_component_id] == std::nullopt && m_component_operators[_component_id] == std::nullopt, "EntityManager -> Registering component that was already registered, id: {}", _component_id);

        ComponentOperators component_operators;

        component_operators.update_component_function =
            [](entityx::Entity& _entity, ComponentPayload& _component_payload)
        {

        };

        component_operators.remove_component_function =
            [](entityx::Entity& _entity)
        {
            _entity.remove<ComponentClass>();
        };

        component_operators.serialize_component_function =
            [](entityx::Entity& _entity, ComponentPayload& _component_payload)
        {
            _entity.remove<ComponentClass>();
        };

        m_component_id_to_hash[_component_id]  = ctti::detailed_nameof<ComponentClass>().name().hash();
        MaximumEntityComponents[_component_id] = std::move(component_operators);

        return true;
    }

    /*
    * Create a server-wide entity
    *
    * This action will generate an entity creation request on the runtime, the success of the request
    * depends on the current worker having enough permission to complete the operation
    * The position component is optional, std::nullopt can be passes if no position is intended
    * Each component parameter type must have been previously registered in this manager, this is 
    * required in order to map the local components with the data that the server has, failing to do 
    * it will result in the component not being included in the payload
    *
    * This function does not return the entity/future reference because there is no guarantee that
    * the entity will be owned by the current worker, if some visual representation is required, it's
    * advised to create a local entity that will hold that information/components
    *
    * <receiving a future answer from this request doesn't seems to be necessary, it's the server
    * duty to create or deny the request and usually, if well configured, failing cases are extremely
    * rare>
    */
    template <class... Components>
    bool CreateEntity(Components&&... _components, std::optional<WorldPosition> _position = std::nullopt)
    {
        constexpr std::size_t total_components = sizeof...(Components);

        // TODO: Check permissions

        EntityPayload entity_payload;

        auto new_entity_id = RetrieveNextAvailableEntityId();
        if (!new_entity_id)
        {
            MessageLog().Error("EntityManager -> Failed to request new entity because: Out of entity ids. Too many entities were create in a small amount of time and/or not enough spare entity ids were provided on the worker creation");
            return false;
        }

        entity_payload.component_payloads.reserve(total_components);

        // Extract each component payload
        [](...) {}((ExtractPayloadFromComponent(new_entity_id.value(), std::forward<Components>(_components), entity_payload.component_payloads), 0)...);

        m_worker.RequestAddEntity(new_entity_id.value(), std::move(entity_payload));

        return true;
    }

    /*
    * Create a local entity
    *
    * This function will create a local entity that is NOT synchronized with the server and only exist
    * on the current worker view space
    * This entity can be used normally as if it was a replicated entity, all operations inside this manager
    * work as expected, just instead relying on a server request they will apply the request directly, if
    * possible
    *
    * Returns if the local entity creation was successfully
    */
    template <class... Components>
    bool CreateLocalEntity(Components&&... _components, std::optional<WorldPosition> _position = std::nullopt)
    {
        // TODO: Check permissions

        auto new_entityx       = m_ecs_manager.entities.create();
        auto new_entityx_index = new_entityx.id().index();
        
        if (new_entityx_index >= m_entity_infos.size())
        {
            MessageLog().Info("EntityManager -> Expanding the maximum entity infos vector to support up to {} entities", new_entityx_index);
            m_entity_infos.resize(new_entityx_index);
        }

        JaniCriticalOnFail(!m_entity_infos[new_entityx_index].has_value(), "EntityManager -> CreateLocalEntity() is trying to use an index that is already on use!");

        EntityInfo new_entity_info;
        new_entity_info.entityx       = new_entityx;
        new_entity_info.is_pure_local = true;

        m_entity_infos[new_entityx_index] = std::move(new_entity_info);

        [](...) {}((new_entityx.assign<Components>(_components), 0)...);

        return true;
    }

    /*
    * Destroy an entity, be it a local or server-wide entity
    *
    * If the given entity is a local one and valid, this function will always succeed
    * For the case where the provided entity is a server-wide entity, a request to the server
    * will be made
    * To destroy a server-wide entity, the current worker must have enough permission
    *
    * This function returns true if the provided parameters are valid and, if necessary, any
    * server request was made successfully. The entity should be considered destroyed after this
    * (even if in the server
    */
    bool DestroyEntity(const Entity& _entity);

    /*
    * Adds a component to an entity
    *
    * If the given entity is a local one, after this function return you will be able to query
    * the component data directly from the entity
    * If this is a server-wide entity, a request will be made to the server, the success of this
    * request depends on the permissions of the current worker
    *
    * Returns either if the local entity received the new component or if the request to the server
    * was made successfully
    */
    template <class ComponentClass>
    bool AddComponent(const Entity& _entity, ComponentClass&& _component)
    {
        auto entity_info = GetEntityInfoMutable(_entity);
        if (entity_info)
        {
            if (entity_info.value()->is_pure_local)
            {
                entity_info.value()->entityx.assign<ComponentClass>(std::move(_component));
                return true;
            }
            else if(entity_info.value()->server_entity_id.has_value())
            {
                // TODO: Check permissions

                auto component_type_hash = ctti::detailed_nameof<ComponentClass>().name().hash();
                auto component_iter      = m_hash_to_component_id.find(component_type_hash);
                if (component_iter != m_hash_to_component_id.end())
                {
                    ComponentPayload component_payload;
                    if (ExtractPayloadFromComponent(entity_info.value()->server_entity_id.value(), std::move(_component), component_payload))
                    {
                        m_worker.RequestAddComponent(
                            entity_info.value()->server_entity_id.value(), 
                            component_iter->second, 
                            std::move(component_payload));

                        if (m_apply_before_server_acknowledge
                            && IsEntityOwned(_entity)
                            && IsComponentOwned<ComponentClass>())
                        {
                            entity_info.value()->entityx.assign<ComponentClass>(std::move(_component));
                        }

                        return true;
                    }
                }
            }
        }

        return false;
    }

    /*
    * Remove a component from an entity
    *
    * Remove the internal component data
    *
    * Returns either if the local entity had its component removed or if the request to the server
    * was made successfully depending if its a local or server-wide entity
    */
    template <class ComponentClass>
    bool RemoveComponent(const Entity& _entity)
    {
        auto entity_info = GetEntityInfoMutable(_entity);
        if (entity_info)
        {
            if (entity_info.value()->is_pure_local)
            {
                entity_info.value()->entityx.remove<ComponentClass>();
                return true;
            }
            else if (entity_info.value()->server_entity_id.has_value())
            {
                // TODO: Check permissions

                auto component_type_hash = ctti::detailed_nameof<ComponentClass>().name().hash();
                auto component_iter      = m_hash_to_component_id.find(component_type_hash);
                if (component_iter != m_hash_to_component_id.end())
                {
                    m_worker.RequestRemoveComponent(
                        entity_info.value()->server_entity_id.value(),
                        component_iter->second);

                    if (m_apply_before_server_acknowledge
                        && IsEntityOwned(_entity)
                        && IsComponentOwned<ComponentClass>())
                    {
                        entity_info.value()->entityx.remove<ComponentClass>();
                    }

                    return true;
                }
            }
        }

        return false;
    }

    /*
    * Replace a component from an entity
    *
    * Replace the internal component data
    *
    * Returns either if the local entity received the new component or if the request to the server
    * was made successfully depending if its a local or server-wide entity
    */
    template <class ComponentClass, typename ... Args>
    bool ReplaceComponent(const Entity& _entity, Args&& ... _args)
    {
        auto entity_info = GetEntityInfoMutable(_entity);
        if (entity_info)
        {
            if (entity_info.value()->is_pure_local)
            {
                entity_info.value()->entityx.replace<ComponentClass>(std::forward<Args>(_args) ...);
                return true;
            }
            else if (entity_info.value()->server_entity_id.has_value()
                     && IsEntityOwned(entity_info.value()->server_entity_id.value()))
            {
                // TODO: Check permissions

                auto component_type_hash = ctti::detailed_nameof<ComponentClass>().name().hash();
                auto component_iter      = m_hash_to_component_id.find(component_type_hash);
                if (component_iter != m_hash_to_component_id.end())
                {
                    ComponentClass   raw_component(std::forward<Args>(_args) ...);
                    ComponentPayload component_payload;
                    if (ExtractPayloadFromComponent(entity_info.value()->server_entity_id.value(), std::move(raw_component), component_payload))
                    {
                        m_worker.RequestUpdateComponent(
                            entity_info.value()->server_entity_id.value(),
                            component_iter->second,
                            std::move(component_payload));

                        return true;
                    }
                }
            }
        }

        return false;
    }

    /*
    * Indicates that the underlying component for the given entity was updated
    *
    * This function should be used only when automatic component update detection is disabled,
    * by calling this function you prevent the manager from checking the component every frame,
    * potentially saving some considerable performance
    *
    * Internally this function will just compare the current data hash with the old one, if
    * the hashes doesn't match and the entity is a server-wide entity, the update will be
    * replicated to the server
    */
    template <class ComponentClass>
    void AcknowledgeComponentUpdate(const Entity& _entity)
    {
        // TODO: Check permissions

        auto entity_info = GetEntityInfoMutable(_entity);
        if (entity_info 
            && !entity_info.value()->is_pure_local 
            && entity_info.value()->server_entity_id.has_value()
            && IsEntityOwned(entity_info.value()->server_entity_id.value())
            && entity_info.value()->entityx.has_component<ComponentClass>())
        {
            auto component_type_hash = ctti::detailed_nameof<ComponentClass>().name().hash();
            auto component_iter      = m_hash_to_component_id.find(component_type_hash);
            if (component_iter == m_hash_to_component_id.end())
            {
                return;
            }

            auto raw_component = entity_info.value()->entityx.component<ComponentClass>();
            ComponentPayload component_payload;
            if (ExtractPayloadFromComponent(entity_info.value()->server_entity_id.value(), *raw_component, component_payload))
            {
                m_worker.RequestUpdateComponent(
                    entity_info.value()->server_entity_id.value(),
                    component_iter->second,
                    std::move(component_payload));
            }
        }
    }

    /*
    
    */
    template <class ComponentClass>
    bool UpdateInterestQuery(const Entity& _entity, ComponentQuery&& _query)
    {
        return UpdateInterestQuery(_entity, { std::move(_query) });
    }

    /*

    */
    template <class ComponentClass>
    bool UpdateInterestQuery(const Entity& _entity, std::vector<ComponentQuery>&& _queries) // TODO: Use span?!
    {
        auto component_type_hash = ctti::detailed_nameof<ComponentClass>().name().hash();
        auto component_iter      = m_hash_to_component_id.find(component_type_hash);
        if (component_iter != m_hash_to_component_id.end())
        {
            return UpdateInterestQuery(_entity, component_iter->second, std::move(_queries));
        }

        return false;
    }

    /*

    */
    bool UpdateInterestQuery(
        const Entity& _entity,
        ComponentId                   _target_component_id,
        std::vector<ComponentQuery>&& _queries); // TODO: Use span?!

    /*
    
    */
    template <class ComponentClass>
    bool ClearInterestQuery(const Entity& _entity)
    {
        auto component_type_hash = ctti::detailed_nameof<ComponentClass>().name().hash();
        auto component_iter      = m_hash_to_component_id.find(component_type_hash);
        if (component_iter != m_hash_to_component_id.end())
        {
            return ClearInterestQuery(_entity, component_iter->second);
        }

        return false;
    }

    /*

    */
    bool ClearInterestQuery(
        const Entity& _entity,
        ComponentId   _target_component_id);

private:

    template <typename ComponentClass>
    bool ExtractPayloadFromComponent(EntityId _entity_id, ComponentClass&& _component, std::vector<ComponentPayload>& _component_payload_container)
    {
        ComponentPayload new_payload;

        if (ExtractPayloadFromComponent(_entity_id, _component, new_payload))
        {
            _component_payload_container.push_back(std::move(new_payload));
            return true;
        }

        return false;
    }

    template <typename ComponentClass>
    bool ExtractPayloadFromComponent(EntityId _entity_id, ComponentClass&& _component, ComponentPayload& _component_payload)
    {
        auto component_class_hash = ctti::detailed_nameof<ComponentClass>().name().hash();

        auto iter = m_hash_to_component_id.find(component_class_hash);
        if (iter == m_hash_to_component_id.end())
        {
            MessageLog().Error("EntityManager -> Trying to transform a component data into payload but the component isn't registered, the component needs to be registered or a local entity must be used instead, component class was {}", ctti::detailed_nameof<ComponentClass>().name().cppstring());
            return false;
        }

        _component_payload.entity_owner = _entity_id;
        _component_payload.component_id = iter->second;
        const int8_t* component_data = reinterpret_cast<const int8_t*>(&_component);
        _component_payload.component_data.insert(_component_payload.component_data.end(), component_data, component_data + sizeof(ComponentClass));

        return true;
    }

    std::optional<EntityId> RetrieveNextAvailableEntityId() const
    {
        return std::nullopt;
    }

    std::optional<const EntityInfo*> GetEntityInfo(const Entity& _entity) const
    {
        auto entity_id = _entity.GetLocalId();
        if (entity_id && entity_id.value() < m_entity_infos.size() && m_entity_infos[entity_id.value()].has_value())
        {
            return &m_entity_infos[entity_id.value()].value();
        }

        return std::nullopt;
    }

    std::optional<EntityInfo*> GetEntityInfoMutable(const Entity& _entity)
    {
        auto entity_id = _entity.GetLocalId();
        if (entity_id && entity_id.value() < m_entity_infos.size() && m_entity_infos[entity_id.value()].has_value())
        {
            return &m_entity_infos[entity_id.value()].value();
        }

        return std::nullopt;
    }

    void DestroyEntityInfo(const Entity& _entity)
    {
        auto entity_id = _entity.GetLocalId();
        if (entity_id && entity_id.value() < m_entity_infos.size() && m_entity_infos[entity_id.value()].has_value())
        {
            m_entity_infos[entity_id.value()].value().entityx.destroy();
            m_entity_infos[entity_id.value()] = std::nullopt;
        }
    }

    bool IsEntityOwned(const Entity& _entity) const
    {
        auto entity_info = GetEntityInfo(_entity);
        if (entity_info && entity_info.value()->server_entity_id.has_value())
        {
            return IsEntityOwned(entity_info.value()->server_entity_id.value());
        }

        return false;
    }

    bool IsEntityOwned(EntityId _entity_id) const
    {
        return m_worker.IsEntityOwned(_entity_id);
    }

    template<typename ComponentClass>
    bool IsComponentOwned() const
    {
        return false;
    }

    bool IsComponentOwned(ComponentId _component_id) const
    {
        return false;
    }

///////////////////////
public: // CALLBACKS //
///////////////////////

public:

private:

////////////////////////
private: // VARIABLES //
////////////////////////

    Worker& m_worker;

    bool m_apply_before_server_acknowledge = false;

    std::unordered_map<ctti::detail::hash_t, ComponentId>                    m_hash_to_component_id;
    std::array<std::optional<ctti::detail::hash_t>, MaximumEntityComponents> m_component_id_to_hash;
    std::array<std::optional<ComponentOperators>, MaximumEntityComponents>   m_component_operators;

    entityx::EntityX m_ecs_manager;

    std::vector<std::optional<EntityInfo>> m_entity_infos;
};

// Jani
JaniNamespaceEnd(Jani)

#undef DeclareTemplateIntrospectionMethod 
#undef TemplateMethodExistent 
#undef TemplateMethodInexistent