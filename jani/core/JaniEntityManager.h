////////////////////////////////////////////////////////////////////////////////
// Filename: EntityManager.h
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
// Class name: EntityManager
////////////////////////////////////////////////////////////////////////////////
class EntityManager
{
private:

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    EntityManager() {}
    ~EntityManager() {}

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

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
    bool CreateEntity(std::optional<WorldPosition> _position, Components&&... _components)
    {
        constexpr std::size_t total_components = sizeof...(Components);

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

        // TODO: Send entity_payload via worker
        // ...

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
    bool DestroyEntity(Entity _entity);

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
    bool AddComponent(Entity _entity, ComponentClass&& _component)
    {


        return false;
    }

    /*
    
    */
    template <class ComponentClass>
    bool RemoveComponent(Entity _entity)
    {

        return false;
    }

    /*

    */
    template <class ComponentClass, typename ... Args>
    bool ReplaceComponent(Entity _entity, Args&& ... _args)
    {
        ComponentClass(std::forward<Args>(_args) ...);

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
    void AcknowledgeComponentUpdate(Entity _entity)
    {

    }

    /*
    
    */
    template <class ComponentClass>
    bool UpdateInterestQuery(Entity _entity, ComponentQuery&& _query)
    {
        return UpdateInterestQuery(_entity, { std::move(_query) });
    }

    /*

    */
    template <class ComponentClass>
    bool UpdateInterestQuery(Entity _entity, std::vector<ComponentQuery>&& _queries) // TODO: Use span?!
    {
        return false;
    }

    /*
    
    */
    template <class ComponentClass>
    bool ClearInterestQuery(Entity _entity)
    {
        return false;
    }

private:

    template <typename ComponentClass>
    bool ExtractPayloadFromComponent(EntityId _entity_id, const ComponentClass& _component, std::vector<ComponentPayload>& _component_payload_container)
    {
        auto component_class_hash = ctti::detailed_nameof<ComponentClass>().name().hash();

        auto iter = m_hash_to_component_id.find(component_class_hash);
        if (iter == m_hash_to_component_id.end())
        {
            MessageLog().Error("EntityManager -> Trying to transform a component data into payload but the component isn't registered, the component needs to be registered or a local entity must be used instead, component class was {}", ctti::detailed_nameof<ComponentClass>().name().cppstring());
            return false;
        }
        
        ComponentPayload new_payload;
        new_payload.entity_owner     = _entity_id;
        new_payload.component_id     = iter->second;
        const int8_t* component_data = reinterpret_cast<const int8_t*>(&_component);
        new_payload.component_data.insert(new_payload.component_data.end(), component_data, component_data + sizeof(ComponentClass));
        _component_payload_container.push_back(std::move(new_payload));

        return true;
    }

    std::optional<EntityId> RetrieveNextAvailableEntityId() const
    {
        return std::nullopt;
    }

///////////////////////
public: // CALLBACKS //
///////////////////////

public:

private:

////////////////////////
private: // VARIABLES //
////////////////////////

    std::unordered_map<ctti::detail::hash_t, ComponentId>                    m_hash_to_component_id;
    std::array<std::optional<ctti::detail::hash_t>, MaximumEntityComponents> m_component_id_to_hash;
};

// Jani
JaniNamespaceEnd(Jani)