////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorkerInstance.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorkerInstance.h"
#include "JaniBridge.h"

Jani::WorkerInstance::WorkerInstance(
    Bridge&                  _bridge,
    LayerId                  _layer_id,
    Connection<>::ClientHash _client_hash,
    bool                     _is_user)
    : m_bridge(_bridge)
    , m_layer_id(_layer_id)
    , m_client_hash(_client_hash)
    , m_is_user(_is_user)
{
    // The constructor should create a Connection object and send an initial data
    // ...
}

Jani::WorkerInstance::~WorkerInstance()
{

}

Jani::LayerId Jani::WorkerInstance::GetLayerId() const
{
    return m_layer_id;
}

Jani::WorkerId Jani::WorkerInstance::GetId() const
{
    return m_client_hash;
}

Jani::Connection<>::ClientHash Jani::WorkerInstance::GetConnectionClientHash() const
{
    return m_client_hash;
}

bool Jani::WorkerInstance::UseSpatialArea() const
{
    return m_use_spatial_area;
}

bool Jani::WorkerInstance::IsUserInstance() const
{
    return m_is_user;
}

std::pair<uint64_t, uint64_t> Jani::WorkerInstance::GetNetworkTrafficPerSecond() const
{
    return { m_total_data_received_per_second, m_total_data_sent_per_second };
}

void Jani::WorkerInstance::ProcessRequest(const RequestInfo& _request, const RequestPayload& _request_payload, ResponsePayload& _response_payload)
{
    switch (_request.type)
    {
        case RequestType::RuntimeLogMessage:
        {
            auto log_request = _request_payload.GetRequest<Message::RuntimeLogMessageRequest>();

            bool result = m_bridge.OnWorkerLogMessage(
                *this, 
                m_client_hash, 
                log_request.log_level,
                std::move(log_request.log_title),
                std::move(log_request.log_message));

            Message::RuntimeDefaultResponse response = { result };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeReserveEntityIdRange:
        {
            auto entity_id_reserve_request = _request_payload.GetRequest<Message::RuntimeReserveEntityIdRangeRequest>();

            auto result = m_bridge.OnWorkerReserveEntityIdRange(
                *this,
                m_client_hash,
                entity_id_reserve_request.total_ids);

            Message::RuntimeReserveEntityIdRangeResponse response = 
            { 
                result.has_value(),
                result ? result.value() : 0, 
                result ? result.value() + entity_id_reserve_request.total_ids : 0
            };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeAddEntity:
        {
            auto add_entity_request = _request_payload.GetRequest<Message::RuntimeAddEntityRequest>();

            bool result = m_bridge.OnWorkerAddEntity(
                *this,
                m_client_hash,
                add_entity_request.entity_id,
                add_entity_request.entity_payload);

            Message::RuntimeDefaultResponse response = { result };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeRemoveEntity:
        {
            auto remove_entity_request = _request_payload.GetRequest<Message::RuntimeRemoveEntityRequest>();

            bool result = m_bridge.OnWorkerRemoveEntity(
                *this,
                m_client_hash,
                remove_entity_request.entity_id);

            Message::RuntimeDefaultResponse response = { result };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeAddComponent:
        {
            auto add_component_request = _request_payload.GetRequest<Message::RuntimeAddComponentRequest>();

            bool result = m_bridge.OnWorkerAddComponent(
                *this,
                m_client_hash,
                add_component_request.entity_id, 
                add_component_request.component_id, 
                add_component_request.component_payload);

            Message::RuntimeDefaultResponse response = { result };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeRemoveComponent:
        {
            auto remove_component_request = _request_payload.GetRequest<Message::RuntimeRemoveComponentRequest>();

            bool result = m_bridge.OnWorkerRemoveComponent(
                *this,
                m_client_hash,
                remove_component_request.entity_id,
                remove_component_request.component_id);

            Message::RuntimeDefaultResponse response = { result };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeComponentUpdate:
        {
            auto component_update_request = _request_payload.GetRequest<Message::RuntimeComponentUpdateRequest>();

            bool result = m_bridge.OnWorkerComponentUpdate(
                *this,
                m_client_hash,
                component_update_request.entity_id,
                component_update_request.component_id, 
                component_update_request.component_payload, 
                component_update_request.entity_world_position);

            Message::RuntimeDefaultResponse response = { result };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeWorkerReportAcknowledge:
        {
            auto component_report_acknowledge_request = _request_payload.GetRequest<Message::RuntimeWorkerReportAcknowledgeRequest>();

            m_total_data_received_per_second = component_report_acknowledge_request.total_data_received_per_second;
            m_total_data_sent_per_second     = component_report_acknowledge_request.total_data_sent_per_second;

            Message::RuntimeDefaultResponse response = { true };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeComponentInterestQueryUpdate:
        {
            auto component_queries_update_request = _request_payload.GetRequest<Message::RuntimeComponentInterestQueryUpdateRequest>();

            bool result = m_bridge.OnWorkerComponentInterestQueryUpdate(
                *this,
                m_client_hash,
                component_queries_update_request.entity_id,
                component_queries_update_request.component_id,
                component_queries_update_request.queries);

            Message::RuntimeDefaultResponse response = { result };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeComponentInterestQuery:
        {
            auto component_queries_request = _request_payload.GetRequest<Message::RuntimeComponentInterestQueryRequest>();

            auto query_entries = m_bridge.OnWorkerComponentInterestQuery(
                *this,
                m_client_hash,
                component_queries_request.entity_id,
                component_queries_request.component_id);

            for (auto& query_entry : query_entries)
            {
                Message::RuntimeComponentInterestQueryResponse response;
                response.succeed               = true;
                response.entity_component_mask = query_entry.first;
                response.components_payloads.reserve(query_entry.second.size());

                uint32_t total_accumulated_size = 0;
                for (auto& component_payload : query_entry.second)
                {
                    // Check if the message is getting too big and break it
                    if (total_accumulated_size + component_payload.component_data.size() > 500)
                    {
                        _response_payload.PushResponse(response);
                        response.components_payloads.clear();
                        total_accumulated_size = 0;
                    }

                    response.components_payloads.push_back(std::move(component_payload));

                    total_accumulated_size += response.components_payloads.back().component_data.size();
                }

                if (response.components_payloads.size() > 0)
                {
                    _response_payload.PushResponse(std::move(response));
                }
            }

            break;
        }
        default:
        {
            Jani::MessageLog().Error("WorkerInstance -> Unknown request type received by worker instance");

            break;
        }
    }
}