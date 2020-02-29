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
public:

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
                parent_worker.value()->WaitForNextMessage(_perform_worker_updates);
            }
        }

        operator bool() const
        {
            return request_result;
        }

    protected:

        ResponseCallback(uint32_t _message_index, Worker* _parent_worker)
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
        std::optional<Worker*>                   parent_worker;
        uint32_t                                 message_index   = std::numeric_limits<uint32_t>::max();
        bool                                     on_response_set = false;
        bool                                     request_result  = false;
    };

    using ResponseCallbackType = std::variant
        <
        ResponseCallback<Message::WorkerReserveEntityIdRangeResponse>,
        ResponseCallback<Message::WorkerAuthenticationResponse>,
        ResponseCallback<Message::WorkerDefaultResponse>
        >;

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Worker(LayerHash _layer_id_hash);
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

protected:

    /*
    
    */
    template<typename ResponseType>
    void RegisterResponseCallback(uint32_t _message_index, const ResponseCallback<ResponseType>& _response)
    {
        m_response_callbacks.push_back({ _message_index, _response });
    }

    /*
    
    */
    void WaitForNextMessage(bool _perform_worker_updates)
    {
        auto current_message_index = m_current_message_index;
        while (IsConnected() && current_message_index == m_current_message_index) // TODO: Included check for timeout
        {
            if (_perform_worker_updates)
            {
                Update(5);
            }

            // If timed out, call the callback with the timeout argument as true
            // ...

            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }

public:

    /*
    * Perform the authentication of this worker with the server
    * Only after this worker is authenticated it will be able to interact
    * with the game server
    */
    ResponseCallback<Message::WorkerAuthenticationResponse> RequestAuthentication();

    /*
    * Send a log message to the game server
    * This is only available on production builds and its basically 
    * a noop when live
    */
    ResponseCallback<Message::WorkerDefaultResponse> RequestLogMessage(
        WorkerLogLevel     _log_level, 
        const std::string& _title, 
        const std::string& _message);

    /*
    * Request the game server to reserve a range of entity ids for this
    * worker
    * This operation can only succeed if this worker has the rights to
    * request and manage entities
    */
    ResponseCallback<Message::WorkerReserveEntityIdRangeResponse> RequestReserveEntityIdRange(uint32_t _total);

    /*
    * Request the game server to add a certain entity and a payload
    * containing its component info
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::WorkerDefaultResponse> RequestAddEntity(
        EntityId      _entity_id, 
        EntityPayload _entity_payload);

    /*
    * Request the game server to remove a certain entity
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::WorkerDefaultResponse> RequestRemoveEntity(EntityId _entity_id);

    /*
    * Request the game server to add a certain component to an entity
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::WorkerDefaultResponse> RequestAddComponent(
        EntityId         _entity_id, 
        ComponentId      _component_id, 
        ComponentPayload _component_payload);

    /*
    * Request the game server to remove a certain component from an entity
    * This operation is dependent on the permissions of this worker
    */
    ResponseCallback<Message::WorkerDefaultResponse> RequestRemoveComponent(
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
    ResponseCallback<Message::WorkerDefaultResponse> RequestUpdateComponent(
        EntityId                     _entity_id,
        ComponentId                  _component_id,
        ComponentPayload             _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

    /*
    * The main update function
    * Use this to process current component data
    */
    virtual void Update(uint32_t _time_elapsed_ms);

private:

////////////////////////
private: // VARIABLES //
////////////////////////

    std::unique_ptr<Connection<>> m_bridge_connection;
    RequestManager                m_request_manager;

    uint32_t  m_current_message_index    = 0;
    uint32_t  m_received_message_index   = 0; // From where we are reading on m_response_callbacks
    uint32_t  m_received_message_counter = 0; // What is the next response index
    LayerHash m_layer_id_hash          = std::numeric_limits<uint32_t>::max();
    bool      m_did_server_timeout     = false;

    std::vector<std::pair<uint32_t, ResponseCallbackType>> m_response_callbacks;
};

// Jani
JaniNamespaceEnd(Jani)