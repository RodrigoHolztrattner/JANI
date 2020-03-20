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
// Class name: Worker
////////////////////////////////////////////////////////////////////////////////
class Worker
{
    friend EntityManager;

public:

    using OnAuthorityGainCallback   = std::function<void(EntityId)>;
    using OnAuthorityLostCallback   = std::function<void(EntityId)>;
    using OnComponentUpdateCallback = std::function<void(EntityId, ComponentId, const ComponentPayload&)>;
    using OnComponentRemoveCallback = std::function<void(EntityId, ComponentId)>;
    using OnEntityCreateCallback    = std::function<void(EntityId)>;
    using OnEntityDestroyCallback   = std::function<void(EntityId)>;

    template<typename ResponseType>
    struct ResponseCallback
    {
        friend Worker;

        ResponseCallback() = default;

        ResponseCallback& OnResponse(std::function<void(const ResponseType&, bool)> _response_callback)
        {
            assert(!on_response_set);

            request_result    = true;
            response_callback = std::move(_response_callback);

            if (parent_worker)
            {
                parent_worker.value()->RegisterResponseCallback(message_index, *this);
            }

            on_response_set = true;

            return *this;
        }

        void WaitResponse(bool _perform_worker_updates = true)
        {
            if (parent_worker)
            {
                parent_worker.value()->WaitForNextMessage(message_index, _perform_worker_updates);
            }
        }

        operator bool() const
        {
            return request_result;
        }

    protected:

        ResponseCallback(RequestInfo::RequestIndex _message_index, Worker* _parent_worker)
            : message_index(_message_index)
            , parent_worker(_parent_worker)
        {
        }

        void Call(const ResponseType& _response, bool _timeout)
        {
            if (response_callback)
            {
                response_callback(_response, _timeout);
            }
        }

        std::function<void(const ResponseType&, bool)> response_callback;
        std::optional<Worker*>                         parent_worker;
        RequestInfo::RequestIndex                      message_index   = std::numeric_limits<RequestInfo::RequestIndex>::max();
        bool                                           on_response_set = false;
        bool                                           request_result  = false;
    };

    using ResponseCallbackType = std::variant
        <
        ResponseCallback<Message::RuntimeReserveEntityIdRangeResponse>,
        ResponseCallback<Message::RuntimeAuthenticationResponse>,
        ResponseCallback<Message::RuntimeDefaultResponse>
        >;

private:

    struct EntityInfo
    {
        ComponentMask                                                                           component_mask;          // Indicate all the components this entity has
        ComponentMask                                                                           owned_component_mask;    // Indicate the components that are owned by the current worker on its layer
        ComponentMask                                                                           interest_component_mask; // Indicate the components that were filled because interest was requested
        std::array<std::vector<ComponentQuery>, MaximumEntityComponents>                        component_queries;
        std::array<std::chrono::time_point<std::chrono::steady_clock>, MaximumEntityComponents> component_queries_time;
        bool                                                                                    is_owned = false;

        // This time is used when an entity is pure interest-based (only exist because of interest queries)
        // If the update timestamp timeout (according to this worker interest timeout settings), it will be removed
        // This does not affect entities that have at least one component owned by this worker
        std::chrono::time_point<std::chrono::steady_clock> last_update_received_timestamp = std::chrono::steady_clock::now();

        /*
        * Returns if this entity is interest pure (only exist because of interest queries)
        */
        bool IsInterestPure() const
        {
            // The first part is required because a worker can own an entity layer the doesn't have components
            return owned_component_mask.count() == 0 && interest_component_mask.count() > 0;
        }
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Worker(LayerId _layer_id);
    ~Worker();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Create and connects this worker with its runtime server
    * Must be called in order to start operating as a worker
    */
    bool InitializeWorker(
        std::string _server_address, 
        uint32_t    _server_port);

    /*
    * Returns if this worker is connected to the game server
    */
    bool IsConnected() const;

    /*
    * The main update function
    * Use this to process current component data
    */
    void Update(uint32_t _time_elapsed_ms);

///////////////////////
public: // CALLBACKS //
///////////////////////

    void RegisterOnAuthorityGainCallback(OnAuthorityGainCallback _callback);
    void RegisterOnAuthorityLostCallback(OnAuthorityLostCallback _callback);
    void RegisterOnComponentUpdateCallback(OnComponentUpdateCallback _callback);
    void RegisterOnComponentRemoveCallback(OnComponentRemoveCallback _callback);
    void RegisterOnEntityCreateCallback(OnEntityCreateCallback _callback);
    void RegisterOnEntityDestroyCallback(OnEntityDestroyCallback _callback);

protected:

    /*
    
    */
    template<typename ResponseType>
    void RegisterResponseCallback(uint32_t _message_index, const ResponseCallback<ResponseType>& _response)
    {
        m_response_callbacks.insert({ _message_index, _response });
    }

    /*
    
    */
    void WaitForNextMessage(RequestInfo::RequestIndex _message_index, bool _perform_worker_updates);

public:

    /*
    * Perform the authentication of this worker with the server
    * Only after this worker is authenticated it will be able to interact
    * with the game server
    */
    ResponseCallback<Message::RuntimeAuthenticationResponse> RequestAuthentication();

    /*
    * Send a log message to the game server
    * This is only available on production builds and its basically 
    * a noop when live
    */
    ResponseCallback<Message::RuntimeDefaultResponse> RequestLogMessage(
        WorkerLogLevel     _log_level, 
        const std::string& _title, 
        const std::string& _message);

protected:

    /*
    * Request the game server to reserve a range of entity ids for this
    * worker
    * This operation can only succeed if this worker has the rights to
    * request and manage entities
    */
    ResponseCallback<Message::RuntimeReserveEntityIdRangeResponse> RequestReserveEntityIdRange(uint32_t _total);

    /*
    * Request the game server to add a certain entity and a payload
    * containing its component info
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::RuntimeDefaultResponse> RequestAddEntity(
        EntityId                     _entity_id, 
        EntityPayload                _entity_payload, 
        std::optional<WorldPosition> _entity_world_position = std::nullopt);

    /*
    * Request the game server to remove a certain entity
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::RuntimeDefaultResponse> RequestRemoveEntity(EntityId _entity_id);

    /*
    * Request the game server to add a certain component to an entity
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::RuntimeDefaultResponse> RequestAddComponent(
        EntityId         _entity_id, 
        ComponentId      _component_id, 
        ComponentPayload _component_payload);

    /*
    * Request the game server to remove a certain component from an entity
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::RuntimeDefaultResponse> RequestRemoveComponent(
        EntityId         _entity_id,
        ComponentId      _component_id);

    /*
    * Request the game server to update a certain component
    * Optionally, if this worker updates the main entity position it can also
    * send the actual position in the game world to generate the appropriated
    * server-side position events
    * Requires this worker to be authoritative against the given component
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::RuntimeDefaultResponse> RequestUpdateComponent(
        EntityId                     _entity_id,
        ComponentId                  _component_id,
        ComponentPayload             _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

    /*
    * Request the same server to update a component interest query
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::RuntimeDefaultResponse> RequestUpdateComponentInterestQuery(
        EntityId                     _entity_id,
        ComponentId                  _component_id,
        std::vector<ComponentQuery>  _queries);

    /*
    * Returns if the given entity is owned by this worker
    */
    bool IsEntityOwned(EntityId _entity_id) const;

////////////////////////
private: // VARIABLES //
////////////////////////

    std::unique_ptr<Connection<>> m_bridge_connection;
    RequestManager                m_request_manager;

    LayerId  m_layer_id                = std::numeric_limits<LayerId>::max();
    bool     m_did_server_timeout      = false;
    bool     m_use_spatial_area        = false;
    uint32_t m_maximum_entity_limit    = 0;
    uint32_t m_entity_count            = 0;
    uint32_t m_interest_entity_timeout = 3000;

    std::chrono::time_point<std::chrono::steady_clock> m_last_worker_report_timestamp = std::chrono::steady_clock::now();

    std::unordered_map<EntityId, EntityInfo>                            m_entity_id_to_info_map;
    std::unordered_map<RequestInfo::RequestIndex, ResponseCallbackType> m_response_callbacks;

    // Callbacks
    OnAuthorityGainCallback   m_on_authority_gain_callback;
    OnAuthorityLostCallback   m_on_authority_lost_callback;
    OnComponentUpdateCallback m_on_component_update_callback;
    OnComponentRemoveCallback m_on_component_remove_callback;
    OnEntityCreateCallback    m_on_entity_create_callback;
    OnEntityDestroyCallback   m_on_entity_destroy_callback;
};

// Jani
JaniNamespaceEnd(Jani)