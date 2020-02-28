////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorker.h"

Jani::Worker::Worker(LayerHash _layer_id_hash)
    : m_layer_id_hash(_layer_id_hash)
{
}

Jani::Worker::~Worker()
{
}

bool Jani::Worker::InitializeWorker(
    std::string _server_address,
    uint32_t    _server_port)
{
    m_bridge_connection = std::make_unique<Connection<>>(
        0,
        _server_port, 
        _server_address.c_str());
    if (!m_bridge_connection)
    {
        return false;
    }

    return true;
}

bool Jani::Worker::IsConnected() const
{
    if (!m_bridge_connection)
    {
        return false;
    }

    return !m_did_server_timeout;
}

Jani::Worker::ResponseCallback<Jani::Message::WorkerAuthenticationResponse> Jani::Worker::RequestAuthentication()
{
    assert(m_bridge_connection);

    Message::WorkerAuthenticationRequest worker_connection_request;
    worker_connection_request.ip[0]                 = 0;
    worker_connection_request.port                  = 0;
    worker_connection_request.layer_hash            = m_layer_id_hash;
    worker_connection_request.access_token          = -1;
    worker_connection_request.worker_authentication = -1;

    if (m_request_maker.MakeRequest(*m_bridge_connection, Jani::RequestType::WorkerAuthentication, worker_connection_request))
    {
        return ResponseCallback<Message::WorkerAuthenticationResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::WorkerAuthenticationResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::WorkerDefaultResponse> Jani::Worker::RequestLogMessage(
    WorkerLogLevel     _log_level,
    const std::string& _title,
    const std::string& _message)
{
    assert(m_bridge_connection);

    Message::WorkerLogMessageRequest worker_log_request;
    worker_log_request.log_level   = _log_level;
    worker_log_request.log_title   = _title;
    worker_log_request.log_message = _message;

    if (m_request_maker.MakeRequest(*m_bridge_connection, Jani::RequestType::WorkerLogMessage, worker_log_request))
    {
        return ResponseCallback<Message::WorkerDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::WorkerDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::WorkerReserveEntityIdRangeResponse> Jani::Worker::RequestReserveEntityIdRange(uint32_t _total)
{
    assert(m_bridge_connection);

    Message::WorkerReserveEntityIdRangeRequest reserver_entity_id_range_request;
    reserver_entity_id_range_request.total_ids = _total;

    if (m_request_maker.MakeRequest(*m_bridge_connection, Jani::RequestType::WorkerReserveEntityIdRange, reserver_entity_id_range_request))
    {
        return ResponseCallback<Message::WorkerReserveEntityIdRangeResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::WorkerReserveEntityIdRangeResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::WorkerDefaultResponse> Jani::Worker::RequestAddEntity(
    EntityId      _entity_id,
    EntityPayload _entity_payload)
{
    assert(m_bridge_connection);

    Message::WorkerAddEntityRequest add_entity_request;
    add_entity_request.entity_id      = _entity_id;
    add_entity_request.entity_payload = std::move(_entity_payload);

    if (m_request_maker.MakeRequest(*m_bridge_connection, Jani::RequestType::WorkerAddEntity, add_entity_request))
    {
        return ResponseCallback<Message::WorkerDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::WorkerDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::WorkerDefaultResponse> Jani::Worker::RequestRemoveEntity(EntityId _entity_id)
{
    assert(m_bridge_connection);

    Message::WorkerRemoveEntityRequest remove_entity_request;
    remove_entity_request.entity_id = _entity_id;

    if (m_request_maker.MakeRequest(*m_bridge_connection, Jani::RequestType::WorkerRemoveEntity, remove_entity_request))
    {
        return ResponseCallback<Message::WorkerDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::WorkerDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::WorkerDefaultResponse> Jani::Worker::RequestAddComponent(
    EntityId         _entity_id,
    ComponentId      _component_id,
    ComponentPayload _component_payload)
{
    assert(m_bridge_connection);

    Message::WorkerAddComponentRequest add_component_request;
    add_component_request.entity_id         = _entity_id;
    add_component_request.component_id      = _component_id;
    add_component_request.component_payload = std::move(_component_payload);

    if (m_request_maker.MakeRequest(*m_bridge_connection, Jani::RequestType::WorkerAddComponent, add_component_request))
    {
        return ResponseCallback<Message::WorkerDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::WorkerDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::WorkerDefaultResponse> Jani::Worker::RequestRemoveComponent(
    EntityId         _entity_id,
    ComponentId      _component_id)
{
    assert(m_bridge_connection);

    Message::WorkerRemoveComponentRequest remove_component_request;
    remove_component_request.entity_id    = _entity_id;
    remove_component_request.component_id = _component_id;

    if (m_request_maker.MakeRequest(*m_bridge_connection, Jani::RequestType::WorkerRemoveComponent, remove_component_request))
    {
        return ResponseCallback<Message::WorkerDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::WorkerDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::WorkerDefaultResponse> Jani::Worker::RequestUpdateComponent(
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    ComponentPayload             _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    assert(m_bridge_connection);

    Message::WorkerComponentUpdateRequest component_update_request;
    component_update_request.entity_id             = _entity_id;
    component_update_request.component_id          = _component_id;
    component_update_request.component_payload     = std::move(_component_payload);
    component_update_request.entity_world_position = _entity_world_position;

    if (m_request_maker.MakeRequest(*m_bridge_connection, Jani::RequestType::WorkerComponentUpdate, component_update_request))
    {
        return ResponseCallback<Message::WorkerDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::WorkerDefaultResponse>();
}

void Jani::Worker::Update(uint32_t _time_elapsed_ms)
{
    if (m_bridge_connection)
    {
        m_bridge_connection->Update();

        m_bridge_connection->DidTimeout(
            [&](std::optional<Jani::Connection<>::ClientHash> _client_hash)
            {
                m_did_server_timeout = true;
            });

        m_request_maker.CheckResponses(
            *m_bridge_connection,
            [&](const Jani::Request& _original_request, const Jani::RequestResponse& _response) -> void
            {
                ResponseCallbackType* response_callback = nullptr;
                uint32_t              message_index     = 0;

                if (m_response_callbacks.size() > m_received_message_index)
                {
                    response_callback = &m_response_callbacks[m_received_message_index].second;
                    message_index     = m_response_callbacks[m_received_message_index].first;

                    if (m_received_message_index == message_index)
                    {
                        m_received_message_index++;
                    }
                }

                // WorkerDefaultResponse
                switch (_original_request.type)
                {
                    case Jani::RequestType::WorkerAuthentication:
                    {
                        auto response = _response.GetResponse<Jani::Message::WorkerAuthenticationResponse>();
                        auto callback = std::get_if<ResponseCallback<Message::WorkerAuthenticationResponse>>((response_callback));
                        if (callback)
                        {
                            callback->Call(response, false);
                        }
                        break;
                    }
                    case Jani::RequestType::WorkerLogMessage:
                    {
                        auto response = _response.GetResponse<Jani::Message::WorkerDefaultResponse>();
                        auto callback = std::get_if<ResponseCallback<Message::WorkerDefaultResponse>>((response_callback));
                        if (callback)
                        {
                            callback->Call(response, false);
                        }
                        break;
                    }
                    case Jani::RequestType::WorkerReserveEntityIdRange:
                    {
                        auto response = _response.GetResponse<Jani::Message::WorkerReserveEntityIdRangeResponse>();
                        auto callback = std::get_if<ResponseCallback<Message::WorkerReserveEntityIdRangeResponse>>((response_callback));
                        if (callback)
                        {
                            callback->Call(response, false);
                        }
                        break;
                    }
                    case Jani::RequestType::WorkerAddEntity:
                    case Jani::RequestType::WorkerRemoveEntity:
                    case Jani::RequestType::WorkerAddComponent:
                    case Jani::RequestType::WorkerRemoveComponent:
                    case Jani::RequestType::WorkerComponentUpdate:
                    {
                        auto response = _response.GetResponse<Jani::Message::WorkerDefaultResponse>();
                        auto callback = std::get_if<ResponseCallback<Message::WorkerDefaultResponse>>((response_callback));
                        if (callback)
                        {
                            callback->Call(response, false);
                        }
                        break;
                    }
                    default:
                    {
                        std::cout << "Worker -> Received invalid response type from server!" << std::endl;
                        break;
                    }
                }

                // Check if we received all pending messages
                if (m_current_message_index == ++m_received_message_counter)
                {
                    // We are free to reset the message indexes
                    m_received_message_counter = 0;
                    m_current_message_index    = 0;
                    m_received_message_index   = 0;
                    m_response_callbacks.clear();
                }
            });
    }
}