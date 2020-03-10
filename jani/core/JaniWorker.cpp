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

void Jani::Worker::RegisterOnAuthorityGainCallback(OnAuthorityGainCallback _callback)
{
    m_on_authority_gain_callback = _callback;
}

void Jani::Worker::RegisterOnAuthorityLostCallback(OnAuthorityLostCallback _callback)
{
    m_on_authority_lost_callback = _callback;
}

void Jani::Worker::RegisterOnComponentAddedCallback(OnComponentAddedCallback _callback)
{
    m_on_component_added_callback = _callback;
}

void Jani::Worker::RegisterOnComponentRemovedCallback(OnComponentRemovedCallback _callback)
{
    m_on_component_removed_callback = _callback;
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

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeAuthentication, worker_connection_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeAuthenticationResponse>(request_result.value(), this);
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

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeLogMessage, worker_log_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(request_result.value(), this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeReserveEntityIdRangeResponse> Jani::Worker::RequestReserveEntityIdRange(uint32_t _total)
{
    assert(m_bridge_connection);

    Message::RuntimeReserveEntityIdRangeRequest reserver_entity_id_range_request;
    reserver_entity_id_range_request.total_ids = _total;

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeReserveEntityIdRange, reserver_entity_id_range_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeReserveEntityIdRangeResponse>(request_result.value(), this);
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

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeAddEntity, add_entity_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(request_result.value(), this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeDefaultResponse> Jani::Worker::RequestRemoveEntity(EntityId _entity_id)
{
    assert(m_bridge_connection);

    Message::RuntimeRemoveEntityRequest remove_entity_request;
    remove_entity_request.entity_id = _entity_id;

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeRemoveEntity, remove_entity_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(request_result.value(), this);
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

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeAddComponent, add_component_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(request_result.value(), this);
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

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeRemoveComponent, remove_component_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(request_result.value(), this);
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

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeComponentUpdate, component_update_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(request_result.value(), this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

Jani::Worker::ResponseCallback<Jani::Message::RuntimeDefaultResponse> Jani::Worker::RequestUpdateComponentInterestQuery(
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    std::vector<ComponentQuery>  _queries)
{
    assert(m_bridge_connection);

    Message::RuntimeComponentInterestQueryUpdateRequest component_update_interest_query_request;
    component_update_interest_query_request.entity_id    = _entity_id;
    component_update_interest_query_request.component_id = _component_id;
    component_update_interest_query_request.queries      = _queries;

    auto entity_info_iter = m_server_entity_to_local_map.find(_entity_id);
    if (entity_info_iter != m_server_entity_to_local_map.end())
    {
        entity_info_iter->second.component_queries[_component_id]      = std::move(_queries);
        entity_info_iter->second.component_queries_time[_component_id] = std::chrono::steady_clock::now();
    }

    auto request_result = m_request_manager.MakeRequest(*m_bridge_connection, Jani::RequestType::RuntimeComponentInterestQueryUpdate, component_update_interest_query_request);
    if (request_result)
    {
        return ResponseCallback<Message::RuntimeDefaultResponse>(request_result.value(), this);
    }

    return ResponseCallback<Message::RuntimeDefaultResponse>();
}

void Jani::Worker::Update(uint32_t _time_elapsed_ms)
{
    // NON OPTIMAL ENTITY QUERY CALLBACK

    auto time_now = std::chrono::steady_clock::now();

    for (auto& [entity_id, entity_info] : m_server_entity_to_local_map)
    {
        for (int i = 0; i < entity_info.component_queries.size(); i++)
        {
            auto& component_queries = entity_info.component_queries[i];
            if (component_queries.size() > 0)
            {
                uint32_t frequency = 0;
                for (auto& query : component_queries)
                {
                    frequency = std::max(frequency, query.frequency);
                }

                if (frequency == 0)
                {
                    continue;
                }

                uint32_t necessary_time_diff = 1000 / frequency;

                if (std::chrono::duration_cast<std::chrono::milliseconds>(time_now - entity_info.component_queries_time[i]).count() > necessary_time_diff)
                {
                    Message::RuntimeComponentInterestQueryRequest component_interest_query_request;
                    component_interest_query_request.entity_id    = entity_id;
                    component_interest_query_request.component_id = i;

                    if (!m_request_manager.MakeRequest(
                        *m_bridge_connection,
                        RequestType::RuntimeComponentInterestQuery,
                        component_interest_query_request))
                    {
                    }

                    entity_info.component_queries_time[i] = time_now;
                }
            }
        }
    }

    //

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
                    case Jani::RequestType::RuntimeComponentInterestQuery:
                    {
                        auto response = _response_payload.GetResponse<Jani::Message::RuntimeComponentInterestQueryResponse>();

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
                auto callback_iter = m_response_callbacks.find(_request_info.request_index);
                if (callback_iter != m_response_callbacks.end())
                {
                    response_callback = &callback_iter->second;
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
                    case Jani::RequestType::RuntimeComponentInterestQueryUpdate:
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

                if (response_callback)
                {
                    m_response_callbacks.erase(_request_info.request_index);
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
                        if (entity_iter != m_server_entity_to_local_map.end())
                        {
                            auto& entity = entity_iter->second.entityx;

                            assert(m_on_component_added_callback);
                            m_on_component_added_callback(entity, add_component_request.component_id, add_component_request.component_payload);
                        }

                        break;
                    }
                    case RequestType::WorkerRemoveComponent:
                    {
                        auto remove_component_request = _request_payload.GetRequest<Message::WorkerRemoveComponentRequest>();

                        auto entity_iter = m_server_entity_to_local_map.find(remove_component_request.entity_id);
                        if (entity_iter != m_server_entity_to_local_map.end())
                        {
                            auto& entity = entity_iter->second.entityx;

                            assert(m_on_component_removed_callback);
                            m_on_component_removed_callback(entity, remove_component_request.component_id);
                        }

                        break;
                    }
                    case RequestType::WorkerLayerAuthorityLostImminent:
                    {
                        assert(false);
                        break;
                    }
                    case RequestType::WorkerLayerAuthorityLost:
                    {
                        auto authority_lost_request = _request_payload.GetRequest<Message::WorkerLayerAuthorityLostRequest>();

                        auto entity_iter = m_server_entity_to_local_map.find(authority_lost_request.entity_id);
                        if (entity_iter != m_server_entity_to_local_map.end())
                        {
                            auto& entity = entity_iter->second.entityx;

                            assert(m_on_authority_lost_callback);
                            m_on_authority_lost_callback(entity); // Should this be done after the erase to prevent issues?

                            // Ideally we should not delete the entity but set it as a non-owned entity, this way we
                            // still keep its data in case another entity has interest on it
                            // In this case it would time out naturally if not received interest data in a certain
                            // amount of time

                            m_entity_count--;

                            entity.destroy();

                            m_local_entity_to_server_map.erase(entity);
                            m_server_entity_to_local_map.erase(entity_iter);
                        }

                        break;
                    }
                    case RequestType::WorkerLayerAuthorityGainImminent:
                    {
                        assert(false);
                        break;
                    }
                    case RequestType::WorkerLayerAuthorityGain:
                    {
                        auto authority_gain_request = _request_payload.GetRequest<Message::WorkerLayerAuthorityGainRequest>();

                        auto entity_iter = m_server_entity_to_local_map.find(authority_gain_request.entity_id);
                        if (entity_iter == m_server_entity_to_local_map.end())
                        {
                            auto new_entity = m_ecs_manager.entities.create();

                            m_entity_count++;

                            LocalEntityInfo local_entity_info;
                            local_entity_info.entityx = std::move(new_entity);

                            entity_iter = m_server_entity_to_local_map.insert({ authority_gain_request.entity_id, std::move(local_entity_info) }).first;

                            m_local_entity_to_server_map.insert({ new_entity, authority_gain_request.entity_id });
                        }

                        auto& entity = entity_iter->second.entityx;

                        assert(m_on_authority_gain_callback);
                        m_on_authority_gain_callback(entity);

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
        worker_report.total_data_received_per_second = Connection<>::GetTotalDataReceived();
        worker_report.total_data_sent_per_second     = Connection<>::GetTotalDataSent();

        if (!m_request_manager.MakeRequest(
            *m_bridge_connection,
            RequestType::RuntimeWorkerReportAcknowledge,
            worker_report))
        {
            std::cout << "Worker -> Failed to send worker report" << std::endl;
        }

        std::cout << "Entity count{" << m_entity_count << "}" << std::endl;
    }
}