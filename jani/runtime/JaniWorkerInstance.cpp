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

bool Jani::WorkerInstance::IsOverCapacity() const
{
    return m_is_over_capacity;
}

std::array<std::optional<Jani::EntityId>, 4> Jani::WorkerInstance::GetEntitiesOnAreaLimit() const
{
    return 
    { 
        m_entities_on_area_limit.extreme_top_entity, 
        m_entities_on_area_limit.extreme_right_entity, 
        m_entities_on_area_limit.extreme_left_entity, 
        m_entities_on_area_limit.extreme_bottom_entity 
    };
}

Jani::WorldRect Jani::WorkerInstance::GetWorldRect() const
{
    return m_area;
}

void Jani::WorkerInstance::ResetOverCapacityFlag()
{
    m_is_over_capacity = false;
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

            m_area                = component_report_acknowledge_request.worker_rect ? component_report_acknowledge_request.worker_rect.value() : WorldRect();
            m_total_over_capacity = component_report_acknowledge_request.total_entities_over_capacity;
            m_is_over_capacity    = !m_is_user ? m_total_over_capacity > 0 : false;

            if (m_is_over_capacity)
            {
                std::cout << "Worker Instance -> A worker is over its entity capacity" << std::endl;
            }

            m_entities_on_area_limit.extreme_top_entity    = component_report_acknowledge_request.extreme_top_entity;
            m_entities_on_area_limit.extreme_right_entity  = component_report_acknowledge_request.extreme_right_entity;
            m_entities_on_area_limit.extreme_left_entity   = component_report_acknowledge_request.extreme_left_entity;
            m_entities_on_area_limit.extreme_bottom_entity = component_report_acknowledge_request.extreme_bottom_entity;

            Message::RuntimeDefaultResponse response = { true };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        case RequestType::RuntimeComponentQuery:
        {
            auto component_query_request = _request_payload.GetRequest<Message::RuntimeComponentQueryRequest>();

#if 0
            bool result = m_bridge.OnWorkerComponentQuery(
                m_client_hash,
                component_update_request.entity_id,
                component_update_request.component_id,
                component_update_request.component_payload,
                component_update_request.entity_world_position);
#endif
            bool result = false;

            Message::RuntimeDefaultResponse response = { result };
            {
                _response_payload.PushResponse(std::move(response));
            }

            break;
        }
        default:
        {
            std::cout << "WorkerInstance -> Unknown request type received by worker instance" << std::endl;
            break;
        }
    }
}