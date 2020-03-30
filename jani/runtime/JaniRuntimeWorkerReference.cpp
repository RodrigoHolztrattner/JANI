////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntimeWorkerReference.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntimeWorkerReference.h"
#include "JaniRuntimeBridge.h"

Jani::RuntimeWorkerReference::RuntimeWorkerReference(
    RuntimeBridge&           _bridge,
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

Jani::RuntimeWorkerReference::~RuntimeWorkerReference()
{

}

Jani::LayerId Jani::RuntimeWorkerReference::GetLayerId() const
{
    return m_layer_id;
}

Jani::WorkerId Jani::RuntimeWorkerReference::GetId() const
{
    return m_client_hash;
}

Jani::Connection<>::ClientHash Jani::RuntimeWorkerReference::GetConnectionClientHash() const
{
    return m_client_hash;
}

bool Jani::RuntimeWorkerReference::UseSpatialArea() const
{
    return m_use_spatial_area;
}

bool Jani::RuntimeWorkerReference::IsUserInstance() const
{
    return m_is_user;
}

std::pair<uint64_t, uint64_t> Jani::RuntimeWorkerReference::GetNetworkTrafficPerSecond() const
{
    return { m_total_data_received_per_second, m_total_data_sent_per_second };
}

void Jani::RuntimeWorkerReference::ProcessRequest(const RequestInfo& _request, const RequestPayload& _request_payload, ResponsePayload& _response_payload)
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

            break;
        }
        case RequestType::RuntimeRemoveEntity:
        {
            auto remove_entity_request = _request_payload.GetRequest<Message::RuntimeRemoveEntityRequest>();

            bool result = m_bridge.OnWorkerRemoveEntity(
                *this,
                m_client_hash,
                remove_entity_request.entity_id);

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
                std::move(component_update_request.component_payload),
                component_update_request.entity_world_position);

            break;
        }
        case RequestType::RuntimeWorkerReportAcknowledge:
        {
            auto component_report_acknowledge_request = _request_payload.GetRequest<Message::RuntimeWorkerReportAcknowledgeRequest>();

            m_total_data_received_per_second = component_report_acknowledge_request.total_data_received_per_second;
            m_total_data_sent_per_second     = component_report_acknowledge_request.total_data_sent_per_second;

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

            break;
        }
        default:
        {
            Jani::MessageLog().Error("RuntimeWorkerReference -> Unknown request type received by worker instance");

            break;
        }
    }
}