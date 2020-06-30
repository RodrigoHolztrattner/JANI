////////////////////////////////////////////////////////////////////////////////
// Filename: EntityManager.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "..\JaniInternal.h"
#include "JaniWorker.h" // Necessary since we use templated functions

#include <optional>
#include <type_traits>
#include <concepts>
#include <span>

///////////////
// NAMESPACE //
///////////////

template<typename T, class Archive>
concept has_deserialize_with_version_function = requires (T t, Archive & a, std::uint32_t const version) { t.template CEREAL_LOAD_FUNCTION_NAME<Archive>(a, version); };

template<typename T, class Archive>
concept has_serialize_with_version_function = requires (T t, Archive & a, std::uint32_t const version) { t.template CEREAL_SAVE_FUNCTION_NAME<Archive>(a, version); };

template<typename T>
concept has_get_entity_world_position = requires { T::GetEntityWorldPosition; };

template<typename T, class InputArchive, class OutputArchive>
concept ValidSerializableComponent = has_deserialize_with_version_function<T, InputArchive> && has_serialize_with_version_function<T, OutputArchive>;

template<typename T, class InputArchive, class OutputArchive>
concept InvalidSerializableComponent = !has_deserialize_with_version_function<T, InputArchive> || !has_serialize_with_version_function<T, OutputArchive>;

template<typename T>
concept ComponentWithWorldPositionGetter = has_get_entity_world_position<T>;

template<typename T>
concept ComponentWithoutWorldPositionGetter = !has_get_entity_world_position<T>;

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

    using OnComponentUpdateFunction        = std::function<bool(entityx::Entity&, const ComponentPayload&)>;
    using OnComponentRemoveFunction        = std::function<bool(entityx::Entity&)>;
    using OnComponentSerializeFunction     = std::function<bool(entityx::Entity&, ComponentPayload&)>;
    using OnComponentWorldPositionFunction = std::function<WorldPosition(entityx::Entity&)>;

    struct ComponentOperators
    {
        OnComponentUpdateFunction        update_component_function;
        OnComponentRemoveFunction        remove_component_function;
        OnComponentSerializeFunction     serialize_component_function;
        OnComponentWorldPositionFunction component_world_position_function;
    };

public:

    using OnEntityCreatedCallback   = std::function<void(Entity)>;
    using OnEntityDestroyedCallback = std::function<void(Entity)>;

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    EntityManager(Worker& _worker);
    ~EntityManager();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Register a callback for when an entity is created
    */
    void RegisterOnEntityCreatedCallback(OnEntityCreatedCallback _callback)
    {
        m_on_entity_created_callback = std::move(_callback);
    }

    /*
    * Register a callback for when an entity is destroyed
    */
    void RegisterOnEntityDestroyedCallback(OnEntityDestroyedCallback _callback)
    {
        m_on_entity_destroyed_callback = std::move(_callback);
    }

    /*
    * Register a component
    *
    * Optionally accepts a component ID if this component should be replicated
    *
    * This function in particular requires that the component have a serialize() function
    */
    template <typename ComponentClass, 
        typename InputArchive                                                     = cereal::BinaryInputArchive,
        typename OutputArchive                                                    = cereal::BinaryOutputArchive,
        ValidSerializableComponent<InputArchive, OutputArchive> SerializableCheck = ComponentClass>
    bool RegisterComponent(std::optional<ComponentId> _component_id = std::nullopt)
    {
        auto component_class_hash = GetHashFromComponentClass<ComponentClass>();

        if (_component_id)
        {
            auto component_id = _component_id.value();

            if (component_id >= MaximumEntityComponents)
            {
                MessageLog().Error("EntityManager -> Trying to register a component but the id provided is out of range: {}", component_id);
                return false;
            }

            JaniWarnOnFail(m_component_id_to_hash[component_id] == std::nullopt, "EntityManager -> Registering component that was already registered, id: {}", component_id);
        }
        else
        {
            JaniWarnOnFail(m_component_operators.find(component_class_hash) == m_component_operators.end(), "EntityManager -> Registering component that was already registered, hash: {}", component_class_hash)
        }

        ComponentOperators component_operators;

        component_operators.update_component_function = 
            [](entityx::Entity& _entity, const ComponentPayload& _component_payload) -> bool
        {
            ComponentClass raw_component;

            StreamVectorWrap<char> data_buffer(nonstd::span<char>(_component_payload.component_data.data(), _component_payload.component_data.data() + _component_payload.component_data.size()));
            std::istream           stream(&data_buffer);

            try
            {
                InputArchive archive(stream);
                archive(raw_component);
            }
            catch (...)
            {
                return false;
            }

            if (_entity.has_component<ComponentClass>())
            {
                _entity.replace<ComponentClass>(std::move(raw_component));
            }
            else
            {
                _entity.assign<ComponentClass>(std::move(raw_component));
            }

            return true;
        };

        component_operators.remove_component_function =
            [](entityx::Entity& _entity) -> bool
        {
            _entity.remove<ComponentClass>();

            return true;
        };

        component_operators.serialize_component_function =
            [](entityx::Entity& _entity, ComponentPayload& _component_payload) -> bool
        {
            if (!_entity.has_component<ComponentClass>())
            {
                return false;
            }

            std::array<char, 512>  temp_buffer;
            StreamVectorWrap<char> data_buffer(nonstd::span<char>(temp_buffer.data(), temp_buffer.data() + temp_buffer.size()));
            std::ostream           stream(&data_buffer);

            auto component_handle = _entity.component<ComponentClass>();

            try
            {
                OutputArchive archive(stream);
                archive(*component_handle);
            }
            catch (...)
            {
                return false;
            }

            _component_payload.component_data.insert(_component_payload.component_data.end(), temp_buffer.data(), temp_buffer.data() + stream.tellp());

            return true;
        };

        if (_component_id.has_value())
        {
            // Check if this component provide an entity world position retrieve method
            CheckComponentWorldPosition(_component_id, component_operators);

            m_hash_to_component_id.insert({ component_class_hash, _component_id.value() });
            m_component_id_to_hash[_component_id.value()] = component_class_hash;
        }

        m_component_operators[component_class_hash]  = std::move(component_operators);

        return true;
    }

    /*
    * Returns the component id for an already registered component
    *
    * This functions assumes that the component was registered previously, returning
    * std::numeric_limits<ComponentId>::max() if it's not found
    */
    template <class ComponentClass>
    ComponentId GetRegisteredComponentId()
    {
        auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
        auto component_iter      = m_hash_to_component_id.find(component_type_hash);
        if (component_iter != m_hash_to_component_id.end())
        {
            return component_iter->second;
        }

        MessageLog().Error("EntityManager -> Calling GetRegisteredComponentId() with an unregistered component: {}", GetNameFromComponentClass<ComponentClass>());
        return std::numeric_limits<ComponentId>::max();
    }

    /*
    * Register a component
    *
    * Optionally accepts a component ID if this component should be replicated
    *
    * This function in particular requires the component to be memcopyable via
    * an std::memcpy function
    */
    template <typename ComponentClass,
        typename InputArchive                                                       = cereal::BinaryInputArchive,
        typename OutputArchive                                                      = cereal::BinaryOutputArchive,
        InvalidSerializableComponent<InputArchive, OutputArchive> SerializableCheck = ComponentClass>
        bool RegisterComponent(std::optional<ComponentId> _component_id = std::nullopt)
    {
        auto component_class_hash = GetHashFromComponentClass<ComponentClass>();

        if (_component_id)
        {
            auto component_id = _component_id.value();

            if (component_id >= MaximumEntityComponents)
            {
                MessageLog().Error("EntityManager -> Trying to register a component but the id provided is out of range: {}", component_id);
                return false;
            }

            JaniWarnOnFail(m_component_id_to_hash[component_id] == std::nullopt, "EntityManager -> Registering component that was already registered, id: {}", component_id);
        }
        else
        {
            JaniWarnOnFail(m_component_operators.find(component_class_hash) == m_component_operators.end(), "EntityManager -> Registering component that was already registered, hash: {}", component_class_hash)
        }

        ComponentOperators component_operators;

        component_operators.update_component_function = 
            [](entityx::Entity& _entity, const ComponentPayload& _component_payload) -> bool
        {
            ComponentClass raw_component;

            if (_component_payload.component_data.size() != sizeof(ComponentClass))
            {
                return false;
            }

            std::memcpy(&raw_component, _component_payload.component_data.data(), _component_payload.component_data.size());

            if (_entity.has_component<ComponentClass>())
            {
                _entity.replace<ComponentClass>(std::move(raw_component));
            }
            else
            {
                _entity.assign<ComponentClass>(std::move(raw_component));
            }

            return true;
        };

        component_operators.remove_component_function =
            [](entityx::Entity& _entity) -> bool
        {
            _entity.remove<ComponentClass>();

            return true;
        };

        component_operators.serialize_component_function =
            [](entityx::Entity& _entity, ComponentPayload& _component_payload) -> bool
        {
            if (!_entity.has_component<ComponentClass>())
            {
                return false;
            }

            auto    component_handle   = _entity.component<ComponentClass>();
            int8_t* component_data_ptr = reinterpret_cast<int8_t*>(&(*component_handle));

            _component_payload.component_data.insert(_component_payload.component_data.end(), component_data_ptr, component_data_ptr + sizeof(ComponentClass));

            return true;
        };

        if (_component_id.has_value())
        {
            // Check if this component provide an entity world position retrieve method
            CheckComponentWorldPosition(_component_id, component_operators);

            m_hash_to_component_id.insert({ component_class_hash, _component_id.value() });
            m_component_id_to_hash[_component_id.value()] = component_class_hash;
        }

        m_component_operators[component_class_hash] = std::move(component_operators);

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
    bool CreateEntity(Components&&... _components)
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
        std::optional<WorldPosition> entity_world_position;

        // Extract each component payload
        [](...) {}((ExtractPayloadFromComponent(new_entity_id.value(), std::forward<Components>(_components), entity_payload.component_payloads, entity_world_position), 0)...);

        m_worker.RequestAddEntity(new_entity_id.value(), std::move(entity_payload), entity_world_position);

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
    bool CreateEntity(std::span<ComponentPayload> _components, std::optional<WorldPosition> _entity_world_position = std::nullopt)
    {
        // TODO: Check permissions

        EntityPayload entity_payload;

        auto new_entity_id = RetrieveNextAvailableEntityId();
        if (!new_entity_id)
        {
            MessageLog().Error("EntityManager -> Failed to request new entity because: Out of entity ids. Too many entities were create in a small amount of time and/or not enough spare entity ids were provided on the worker creation");
            return false;
        }

        entity_payload.component_payloads.reserve(_components.size());
        for (auto& component_payload : _components)
        {
            entity_payload.component_payloads.push_back(component_payload);
        }

        m_worker.RequestAddEntity(new_entity_id.value(), std::move(entity_payload), _entity_world_position);

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
    std::optional<LocalEntityId> CreateLocalEntity(Components&&... _components)
    {
        // TODO: Check permissions

        auto new_entityx       = m_ecs_manager.entities.create();
        auto new_entityx_index = new_entityx.id().index();
        
        if (new_entityx_index >= m_entity_infos.size())
        {
            MessageLog().Info("EntityManager -> Expanding the maximum entity infos vector to support up to {} entities", new_entityx_index + 1);
            m_entity_infos.resize(new_entityx_index + 1);
        }

        JaniCriticalOnFail(!m_entity_infos[new_entityx_index].has_value(), "EntityManager -> CreateLocalEntity() is trying to use an index that is already on use!");

        EntityInfo new_entity_info;
        new_entity_info.entityx       = new_entityx;
        new_entity_info.is_pure_local = true;

        m_entity_infos[new_entityx_index] = std::move(new_entity_info);

        [](...) {}((new_entityx.assign<Components>(_components), 0)...);

        return new_entityx_index;
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
    /*
    bool CreateEntity(std::span<ComponentPayload> _components, std::optional<WorldPosition> _entity_world_position = std::nullopt)
    {
        // TODO: Check permissions

        auto new_entityx = m_ecs_manager.entities.create();
        auto new_entityx_index = new_entityx.id().index();

        if (new_entityx_index >= m_entity_infos.size())
        {
            MessageLog().Info("EntityManager -> Expanding the maximum entity infos vector to support up to {} entities", new_entityx_index + 1);
            m_entity_infos.resize(new_entityx_index + 1);
        }

        JaniCriticalOnFail(!m_entity_infos[new_entityx_index].has_value(), "EntityManager -> CreateLocalEntity() is trying to use an index that is already on use!");

        EntityInfo new_entity_info;
        new_entity_info.entityx = new_entityx;
        new_entity_info.is_pure_local = true;

        m_entity_infos[new_entityx_index] = std::move(new_entity_info);

        [](...) {}((new_entityx.assign<Components>(_components), 0)...);

        return new_entityx_index;
    }
    */

    /*
    * Returns a local entity with the given identifier
    * Cannot use a const version because the returned entity uses internally this manager on its
    * member functions
    */
    std::optional<Entity> GetLocalEntityFromId(LocalEntityId _local_entity_id);

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
        // TODO: check if the component is owned, if not just add it directly to the entityx object

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

                auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
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

                auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
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

                auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
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
            auto entity_index        = entity_info.value()->entityx.id().index();
            auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
            auto component_iter      = m_hash_to_component_id.find(component_type_hash);
            if (component_iter == m_hash_to_component_id.end())
            {
                return;
            }

            auto raw_component = entity_info.value()->entityx.component<ComponentClass>();
            ComponentPayload             component_payload;
            std::optional<WorldPosition> entity_world_position;
            if (ExtractPayloadFromComponent(entity_info.value()->server_entity_id.value(), *raw_component, component_payload, entity_world_position))
            {
                m_worker.RequestUpdateComponent(
                    entity_info.value()->server_entity_id.value(),
                    component_iter->second,
                    std::move(component_payload),
                    entity_world_position);
            }
        }
    }

    /*
    
    */
    template <class ComponentClass>
    bool UpdateInterestQuery(const Entity& _entity, ComponentQuery _query)
    {
        auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
        auto component_iter      = m_hash_to_component_id.find(component_type_hash);
        if (component_iter != m_hash_to_component_id.end())
        {
            return UpdateInterestQuery(_entity, component_iter->second, { std::move(_query) });
        }

        return false;
    }

    /*

    */
    template <class ComponentClass>
    bool UpdateInterestQuery(const Entity& _entity, std::vector<ComponentQuery>&& _queries) // TODO: Use span?!
    {
        auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
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
        auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
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

///////////////////////
public: // ITERATORS //
///////////////////////

    template <typename T> 
    struct Identity 
    { 
        typedef T type; 
    };

    template <typename ... Components, typename ... Args>
    void ForEach(typename Identity<std::function<void(Jani::Entity _entity, Components&..., Args&...)>>::type _f, Args&... _args)
    {
        m_ecs_manager.entities.each<Components...>(
            [&](entityx::Entity _entity, Components&... _components)
            {
                auto entity_id = _entity.id().index();
                _f({ *this, entity_id }, _components ..., _args ...);
            });
    }

private:

    template <typename ComponentClass, ComponentWithWorldPositionGetter WorldPositionCheck = ComponentClass>
        void CheckComponentWorldPosition(ComponentId _component_id, ComponentOperators& _component_operators)
    {
        _component_operators.component_world_position_function = [](entityx::Entity& _entity) -> WorldPosition
        {
            // TODO: Check if the entity has the component?
            return _entity.component<ComponentClass>()->GetEntityWorldPosition();
        };

        m_total_components_with_world_position++;
    }

    template <typename ComponentClass, ComponentWithoutWorldPositionGetter WorldPositionCheck = ComponentClass>
        void CheckComponentWorldPosition(ComponentId _component_id, ComponentOperators& _component_operators)
    {
        /* do nothing */
    }

    template <typename ComponentClass>
    bool ExtractPayloadFromComponent(EntityId _entity_id, ComponentClass&& _component, std::vector<ComponentPayload>& _component_payload_container, std::optional<WorldPosition>& _entity_world_position)
    {
        ComponentPayload new_payload;

        if (ExtractPayloadFromComponent(_entity_id, _component, new_payload, _entity_world_position))
        {
            _component_payload_container.push_back(std::move(new_payload));
            return true;
        }

        return false;
    }

    template <typename ComponentClass>
    bool ExtractPayloadFromComponent(EntityId _entity_id, ComponentClass&& _component, ComponentPayload& _component_payload, std::optional<WorldPosition>& _entity_world_position)
    {
        auto component_class_hash = GetHashFromComponentClass<ComponentClass>();

        auto iter = m_hash_to_component_id.find(component_class_hash);
        if (iter == m_hash_to_component_id.end())
        {
            MessageLog().Error("EntityManager -> Trying to transform a component data into payload but the component isn't registered, the component needs to be registered or a local entity must be used instead, component class was {}", GetNameFromComponentClass<ComponentClass>());
            return false;
        }

        auto component_operator_iter = m_component_operators.find(component_class_hash);
        if (component_operator_iter != m_component_operators.end() && component_operator_iter->second.component_world_position_function)
        {
            auto entity_iter = m_server_id_to_local.find(_entity_id);
            if (entity_iter != m_server_id_to_local.end())
            {
                _entity_world_position = component_operator_iter->second.component_world_position_function(entity_iter->second);
            }
        }

        _component_payload.entity_owner = _entity_id;
        _component_payload.component_id = iter->second;
        const int8_t* component_data    = reinterpret_cast<const int8_t*>(&_component);
        _component_payload.component_data.insert(_component_payload.component_data.end(), component_data, component_data + sizeof(ComponentClass));

        return true;
    }

    std::optional<EntityId> RetrieveNextAvailableEntityId(bool _is_waiting_for_reserve_response = false) const;

    std::optional<const EntityInfo*> GetEntityInfo(const Entity& _entity) const;

    std::optional<EntityInfo*> GetEntityInfoMutable(const Entity& _entity);

    void DestroyEntityInfo(const Entity& _entity);

    bool IsEntityOwned(const Entity& _entity) const;

    bool IsEntityOwned(EntityId _entity_id) const;

    template<typename ComponentClass>
    bool IsComponentOwned() const
    {
        auto component_type_hash = GetHashFromComponentClass<ComponentClass>();
        auto component_id_iter = m_hash_to_component_id.find(component_type_hash);
        if (component_id_iter != m_hash_to_component_id.end())
        {
            return IsComponentOwned(component_id_iter->second);
        }

        return false;
    }

    bool IsComponentOwned(ComponentId _component_id) const;

////////////////////////
private: // VARIABLES //
////////////////////////

    Worker& m_worker;

    mutable std::vector<std::pair<EntityId, EntityId>> m_available_entity_id_ranges;

    bool m_apply_before_server_acknowledge = false;

    std::unordered_map<ComponentHash, ComponentId>                    m_hash_to_component_id;
    std::array<std::optional<ComponentHash>, MaximumEntityComponents> m_component_id_to_hash;
    std::unordered_map<ComponentHash, ComponentOperators>             m_component_operators;

    entityx::EntityX m_ecs_manager;

    std::vector<std::optional<EntityInfo>>        m_entity_infos;
    std::unordered_map<EntityId, entityx::Entity> m_server_id_to_local;

    uint32_t m_total_components_with_world_position = 0;

    OnEntityCreatedCallback   m_on_entity_created_callback;
    OnEntityDestroyedCallback m_on_entity_destroyed_callback;
};

// Jani
JaniNamespaceEnd(Jani)