////////////////////////////////////////////////////////////////////////////////
// Filename: JaniClientEntity.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"
#include "JaniEntityManager.h"

#include <entityx/entityx.h>

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

////////////////////////////////////////////////////////////////////////////////
// Class name: ClientEntity
////////////////////////////////////////////////////////////////////////////////
class ClientEntity
{
    friend EntityManager;

    using EntityId = decltype(std::declval<entityx::Entity::Id>().index());

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    ~ClientEntity();

protected:

    ClientEntity(EntityManager& _entity_manager, uint32_t _local_id);

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Returns the local id associated with this entity, if it's valid
    * This index does not represents the server-wide entity id
    */
    std::optional<EntityId> GetLocalId() const
    {
        return m_local_id;
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
    bool DestroyEntity();

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
    bool AddComponent(ComponentClass&& _component)
    {
        return m_entity_manager.get().AddComponent<ComponentClass>(*this, std::move(_component));
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
    bool RemoveComponent()
    {
        return m_entity_manager.get().RemoveComponent<ComponentClass>(*this);
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
    bool ReplaceComponent(Args&& ... _args)
    {
        return m_entity_manager.get().ReplaceComponent<ComponentClass, Args...>(*this, std::move(_args ...));
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
    void AcknowledgeComponentUpdate()
    {
        return m_entity_manager.get().AcknowledgeComponentUpdate<ComponentClass>(*this);
    }

    /*
    
    */
    template <class ComponentClass>
    bool UpdateInterestQuery(ComponentQuery&& _query)
    {
        return m_entity_manager.get().UpdateInterestQuery<ComponentClass>(*this, std::move(_query));
    }

    /*

    */
    template <class ComponentClass>
    bool UpdateInterestQuery(std::vector<ComponentQuery>&& _queries) // TODO: Use span?!
    {
        return m_entity_manager.get().UpdateInterestQuery<ComponentClass>(*this, std::move(_queries));
    }

    /*

    */
    bool UpdateInterestQuery(
        ComponentId                   _target_component_id,
        std::vector<ComponentQuery>&& _queries); // TODO: Use span?!

    /*
    
    */
    template <class ComponentClass>
    bool ClearInterestQuery()
    {
        return m_entity_manager.get().ClearInterestQuery<ComponentClass>(*this);
    }

    /*

    */
    bool ClearInterestQuery(ComponentId _target_component_id);

////////////////////////
private: // VARIABLES //
////////////////////////

    std::reference_wrapper<EntityManager> m_entity_manager;
    std::optional<EntityId>               m_local_id;
};

// Jani
JaniNamespaceEnd(Jani)