////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorker.h"

Jani::Worker::Worker(LayerId _layer_id)
    : m_layer_id(_layer_id)
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

    // m_bridge_connection->SetTimeoutTime(500000000);

    return true;
}

entityx::EntityManager& Jani::Worker::GetEntityManager()
{
    return m_ecs_manager.entities;
}

bool Jani::Worker::IsConnected() const
{
    if (!m_bridge_connection)
    {
        return false;
    }

    return !m_did_server_timeout;
}

void Jani::Worker::ReportEntityPosition(EntityId _entity_id, WorldPosition _position)
{
    m_entities_positions[_entity_id] = _position;
}

void Jani::Worker::RegisterOnComponentAddedCallback(OnComponentAddedCallback _callback)
{
    m_on_component_added_callback = _callback;
}

void Jani::Worker::RegisterOnComponentRemovedCallback(OnComponentRemovedCallback _callback)
{
    m_on_component_removed_callback = _callback;
}

void Jani::Worker::CalculateWorkerProperties(
    std::optional<EntityId>&  _extreme_top_entity,
    std::optional<EntityId>&  _extreme_right_entity,
    std::optional<EntityId>&  _extreme_left_entity,
    std::optional<EntityId>&  _extreme_bottom_entity,
    uint32_t&                 _total_entities_over_capacity,
    std::optional<WorldRect>& _worker_rect) const
{
    if (m_use_spatial_area && m_entities_positions.size() > 0)
    {
        struct EntityPositionInfo
        {
            EntityPositionInfo(WorldPosition _initial_position) : entity_position(_initial_position) {}

            EntityId      entity_index    = std::numeric_limits<EntityId>::max();
            WorldPosition entity_position = { 0, 0 };
        };

        constexpr int32_t  max_coordinate = std::numeric_limits<int32_t>::max() - 1;
        EntityPositionInfo top_entity     = EntityPositionInfo({ 0, -max_coordinate });
        EntityPositionInfo right_entity   = EntityPositionInfo({ -max_coordinate, 0 });
        EntityPositionInfo left_entity    = EntityPositionInfo({ max_coordinate, 0 });
        EntityPositionInfo bottom_entity  = EntityPositionInfo({ 0, max_coordinate });

        for (auto& [entity_id, entity_position] : m_entities_positions)
        {
            if (entity_position.y > top_entity.entity_position.y)
            {
                top_entity.entity_index    = entity_id;
                top_entity.entity_position = entity_position;
            }

            if (entity_position.x > right_entity.entity_position.x)
            {
                right_entity.entity_index    = entity_id;
                right_entity.entity_position = entity_position;
            }

            if (entity_position.x < left_entity.entity_position.x)
            {
                left_entity.entity_index    = entity_id;
                left_entity.entity_position = entity_position;
            }

            if (entity_position.y < bottom_entity.entity_position.y)
            {
                bottom_entity.entity_index    = entity_id;
                bottom_entity.entity_position = entity_position;
            }
        }

        WorldRect worker_area = {
            left_entity.entity_position.x,
            top_entity.entity_position.y,
            right_entity.entity_position.x - left_entity.entity_position.x,
            top_entity.entity_position.y - bottom_entity.entity_position.y };

        if (top_entity.entity_index != std::numeric_limits<EntityId>::max())
        {
            _extreme_top_entity = top_entity.entity_index;
        }

        if (right_entity.entity_index != std::numeric_limits<EntityId>::max()
            && right_entity.entity_index != top_entity.entity_index)
        {
            _extreme_right_entity = right_entity.entity_index;
        }

        if (left_entity.entity_index != std::numeric_limits<EntityId>::max()
            && left_entity.entity_index != right_entity.entity_index
            && left_entity.entity_index != top_entity.entity_index)
        {
            _extreme_left_entity = left_entity.entity_index;
        }

        if (bottom_entity.entity_index != std::numeric_limits<EntityId>::max()
            && bottom_entity.entity_index != right_entity.entity_index
            && bottom_entity.entity_index != top_entity.entity_index
            && bottom_entity.entity_index != left_entity.entity_index)
        {
            _extreme_bottom_entity = bottom_entity.entity_index;
        }

        _worker_rect = worker_area;
    }

    // Check if there is no limit for the entity count
    if (m_maximum_entity_limit == 0)
    {
        return;
    }

    _total_entities_over_capacity = std::max(0, (static_cast<int32_t>(m_entity_count) - static_cast<int32_t>(m_maximum_entity_limit)));
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeAuthenticationResponse> Jani::Worker::RequestAuthentication()
{
    assert(m_bridge_connection);

    Message::RuntimeAuthenticationRequest worker_connection_request;
    worker_connection_request.ip[0]                 = 0;
    worker_connection_request.port                  = 0;
    worker_connection_request.layer_id              = m_layer_id;
    worker_connection_request.access_token          = -1;
    worker_connection_request.worker_authentication = -1;

    if (m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeAuthentication, worker_connection_request))
    {
        return ResponseCallback<Message::RuntimeAuthenticationResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::RuntimeAuthenticationResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeDefaultResponse> Jani::Worker::RequestLogMessage(
    WorkerLogLevel     _log_level,
    const std::string& _title,
    const std::string& _message)
{
    assert(m_bridge_connection);

    Message::RuntimeLogMessageRequest worker_log_request;
    worker_log_request.log_level   = _log_level;
    worker_log_request.log_title   = _title;
    worker_log_request.log_message = _message;

    if (m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeLogMessage, worker_log_request))
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeReserveEntityIdRangeResponse> Jani::Worker::RequestReserveEntityIdRange(uint32_t _total)
{
    assert(m_bridge_connection);

    Message::RuntimeReserveEntityIdRangeRequest reserver_entity_id_range_request;
    reserver_entity_id_range_request.total_ids = _total;

    if (m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeReserveEntityIdRange, reserver_entity_id_range_request))
    {
        return ResponseCallback<Message::RuntimeReserveEntityIdRangeResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::RuntimeReserveEntityIdRangeResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeDefaultResponse> Jani::Worker::RequestAddEntity(
    EntityId      _entity_id,
    EntityPayload _entity_payload)
{
    assert(m_bridge_connection);

    Message::RuntimeAddEntityRequest add_entity_request;
    add_entity_request.entity_id      = _entity_id;
    add_entity_request.entity_payload = std::move(_entity_payload);

    if (m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeAddEntity, add_entity_request))
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeDefaultResponse> Jani::Worker::RequestRemoveEntity(EntityId _entity_id)
{
    assert(m_bridge_connection);

    Message::RuntimeRemoveEntityRequest remove_entity_request;
    remove_entity_request.entity_id = _entity_id;

    if (m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeRemoveEntity, remove_entity_request))
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeDefaultResponse> Jani::Worker::RequestAddComponent(
    EntityId         _entity_id,
    ComponentId      _component_id,
    ComponentPayload _component_payload)
{
    assert(m_bridge_connection);

    Message::RuntimeAddComponentRequest add_component_request;
    add_component_request.entity_id         = _entity_id;
    add_component_request.component_id      = _component_id;
    add_component_request.component_payload = std::move(_component_payload);

    if (m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeAddComponent, add_component_request))
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeDefaultResponse> Jani::Worker::RequestRemoveComponent(
    EntityId         _entity_id,
    ComponentId      _component_id)
{
    assert(m_bridge_connection);

    Message::RuntimeRemoveComponentRequest remove_component_request;
    remove_component_request.entity_id    = _entity_id;
    remove_component_request.component_id = _component_id;

    if (m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeRemoveComponent, remove_component_request))
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeDefaultResponse> Jani::Worker::RequestUpdateComponent(
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    ComponentPayload             _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    assert(m_bridge_connection);

    Message::RuntimeComponentUpdateRequest component_update_request;
    component_update_request.entity_id             = _entity_id;
    component_update_request.component_id          = _component_id;
    component_update_request.component_payload     = std::move(_component_payload);
    component_update_request.entity_world_position = _entity_world_position;

    if (m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeComponentUpdate, component_update_request))
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(m_current_message_index++, this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
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

        m_request_manager.Update(
            *m_bridge_connection,
            [&](auto _client_hash, const Jani::RequestInfo& _request_info, const Jani::ResponsePayload& _response_payload) -> void
            {
                // Check for internal responses
                bool is_internal_response = false;
                switch (_request_info.type)
                {
                    case Jani::RequestType::RuntimeWorkerReportAcknowledge:
                    {
                        is_internal_response = true;
                        break;
                    }
                }

                // Do not continue if this response was handled internally
                if (is_internal_response)
                {
                    return;
                }

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

                // RuntimeDefaultResponse
                switch (_request_info.type)
                {
                    case Jani::RequestType::RuntimeAuthentication:
                    {
                        auto response = _response_payload.GetResponse<Jani::Message::RuntimeAuthenticationResponse>();

                        if (response.succeed)
                        {
                            m_use_spatial_area     = response.use_spatial_area;
                            m_maximum_entity_limit = response.maximum_entity_limit;
                        }

                        auto callback = std::get_if<ResponseCallback<Message::RuntimeAuthenticationResponse>>((response_callback));
                        if (callback)
                        {
                            callback->Call(response, false);
                        }
                        break;
                    }
                    case Jani::RequestType::RuntimeLogMessage:
                    {
                        auto response = _response_payload.GetResponse<Jani::Message::RuntimeDefaultResponse>();
                        auto callback = std::get_if<ResponseCallback<Message::RuntimeDefaultResponse>>((response_callback));
                        if (callback)
                        {
                            callback->Call(response, false);
                        }
                        break;
                    }
                    case Jani::RequestType::RuntimeReserveEntityIdRange:
                    {
                        auto response = _response_payload.GetResponse<Jani::Message::RuntimeReserveEntityIdRangeResponse>();
                        auto callback = std::get_if<ResponseCallback<Message::RuntimeReserveEntityIdRangeResponse>>((response_callback));
                        if (callback)
                        {
                            callback->Call(response, false);
                        }
                        break;
                    }
                    case Jani::RequestType::RuntimeAddEntity:
                    case Jani::RequestType::RuntimeRemoveEntity:
                    case Jani::RequestType::RuntimeAddComponent:
                    case Jani::RequestType::RuntimeRemoveComponent:
                    case Jani::RequestType::RuntimeComponentUpdate:
                    {
                        auto response = _response_payload.GetResponse<Jani::Message::RuntimeDefaultResponse>();
                        auto callback = std::get_if<ResponseCallback<Message::RuntimeDefaultResponse>>((response_callback));
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
            },
            [&](auto                         _client_hash,
                const RequestInfo&           _request_info,
                const Jani::RequestPayload& _request_payload,
                Jani::ResponsePayload&      _response_payload)
            {
                switch (_request_info.type)
                {
                    case RequestType::WorkerAddComponent:
                    {
                        auto add_component_request = _request_payload.GetRequest<Message::WorkerAddComponentRequest>();

                        // Check if we already have the entity created
                        auto entity_iter = m_server_entity_to_local_map.find(add_component_request.entity_id);
                        if (entity_iter == m_server_entity_to_local_map.end())
                        {
                            auto new_entity = m_ecs_manager.entities.create();

                            m_entity_count++;

                            entity_iter = m_server_entity_to_local_map.insert({ add_component_request.entity_id, new_entity }).first;
                            m_local_entity_to_server_map.insert({ new_entity, add_component_request.entity_id });
                        }

                        auto& entity = entity_iter->second;

                        assert(m_on_component_added_callback);
                        m_on_component_added_callback(entity, add_component_request.component_id, add_component_request.component_payload);

                        break;
                    }
                    case RequestType::WorkerRemoveComponent:
                    {
                        auto remove_component_request = _request_payload.GetRequest<Message::WorkerRemoveComponentRequest>();

                        auto entity_iter = m_server_entity_to_local_map.find(remove_component_request.entity_id);
                        if (entity_iter != m_server_entity_to_local_map.end())
                        {
                            auto& entity = entity_iter->second;

                            assert(m_on_component_removed_callback);
                            m_on_component_removed_callback(entity, remove_component_request.component_id);

                            if (entity.component_mask().count() == 0)
                            {
                                m_entity_count--;

                                entity.destroy();

                                m_entities_positions.erase(entity_iter->first);
                                m_local_entity_to_server_map.erase(entity);
                                m_server_entity_to_local_map.erase(entity_iter);
                            }
                        }

                        break;
                    }
                    case RequestType::WorkerLayerAuthorityLossImminent:
                    {
                        break;
                    }
                    case RequestType::WorkerLayerAuthorityLoss:
                    {
                        break;
                    }
                    case RequestType::WorkerLayerAuthorityGainImminent:
                    {
                        break;
                    }
                    case RequestType::WorkerLayerAuthorityGain:
                    {
                        break;
                    }
                    default:
                    {
                        std::cout << "Worker -> Received invalid request from server" << std::endl;

                        break;
                    }
                }

                Message::WorkerDefaultResponse response = { true };
                {
                    _response_payload.PushResponse(std::move(response));
                }
            });
    }

    // Determine if we should send a worker report
    // [[unlikely]]
    if(m_bridge_connection && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_last_worker_report_timestamp).count() > 1000)
    {
        m_last_worker_report_timestamp = std::chrono::steady_clock::now();
     
        Message::RuntimeWorkerReportAcknowledgeRequest worker_report;

        CalculateWorkerProperties(
            worker_report.extreme_bottom_entity, 
            worker_report.extreme_right_entity, 
            worker_report.extreme_left_entity, 
            worker_report.extreme_bottom_entity, 
            worker_report.total_entities_over_capacity, 
            worker_report.worker_rect);

        if (!m_request_manager.MakeRequest(
            *m_bridge_connection,
            RequestType::RuntimeWorkerReportAcknowledge,
            worker_report))
        {
            std::cout << "Worker -> Failed to send worker report" << std::endl;
        }
    }
}