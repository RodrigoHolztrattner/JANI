////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorkerInstance.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorkerInstance.h"
#include "JaniBridge.h"

Jani::WorkerInstance::WorkerInstance(
    Bridge&                  _bridge,
    LayerHash                _layer_hash,
    Connection<>::ClientHash _client_hash,
    bool                     _is_user)
    : m_bridge(_bridge)
    , m_layer_hash(_layer_hash)
    , m_client_hash(_client_hash)
    , m_is_user(_is_user)
{
    // The constructor should create a Connection object and send an initial data
    // ...
}

Jani::WorkerInstance::~WorkerInstance()
{

}

Jani::LayerHash Jani::WorkerInstance::GetLayerHash() const
{
    return m_layer_hash;
}

Jani::WorkerId Jani::WorkerInstance::GetId() const
{
    return m_client_hash;
}

bool Jani::WorkerInstance::IsUserInstance() const
{
    return m_is_user;
}

void Jani::WorkerInstance::ProcessRequest(const Request& _request, cereal::BinaryInputArchive& _request_payload, cereal::BinaryOutputArchive& _response_payload)
{
    switch (_request.type)
    {
        case RequestType::WorkerLogMessage:
        {
            Message::WorkerLogMessageRequest log_request;
            {
                _request_payload(log_request);
            }

            bool result = m_bridge.OnWorkerLogMessage(
                m_client_hash, 
                log_request.log_level,
                std::move(log_request.log_title),
                std::move(log_request.log_message));

            Message::WorkerDefaultResponse response = { result };
            {
                _response_payload(response);
            }

            break;
        }
        case RequestType::WorkerReserveEntityIdRange:
        {
            Message::WorkerReserveEntityIdRangeRequest entity_id_reserve_request;
            {
                _request_payload(entity_id_reserve_request);
            }

            auto result = m_bridge.OnWorkerReserveEntityIdRange(
                m_client_hash,
                entity_id_reserve_request.total_ids);

            Message::WorkerReserveEntityIdRangeResponse response = 
            { 
                result.has_value(),
                result ? result.value() : 0, 
                result ? result.value() + entity_id_reserve_request.total_ids : 0
            };
            {
                _response_payload(response);
            }

            break;
        }
        case RequestType::WorkerAddEntity:
        {
            Message::WorkerAddEntityRequest add_entity_request;
            {
                _request_payload(add_entity_request);
            }

            bool result = m_bridge.OnWorkerAddEntity(
                m_client_hash,
                add_entity_request.entity_id,
                add_entity_request.entity_payload);

            Message::WorkerDefaultResponse response = { result };
            {
                _response_payload(response);
            }

            break;
        }
        case RequestType::WorkerRemoveEntity:
        {
            Message::WorkerRemoveEntityRequest remove_entity_request;
            {
                _request_payload(remove_entity_request);
            }

            bool result = m_bridge.OnWorkerRemoveEntity(
                m_client_hash,
                remove_entity_request.entity_id);

            Message::WorkerDefaultResponse response = { result };
            {
                _response_payload(response);
            }

            break;
        }
        case RequestType::WorkerAddComponent:
        {
            Message::WorkerAddComponentRequest add_component_request;
            {
                _request_payload(add_component_request);
            }

            bool result = m_bridge.OnWorkerAddComponent(
                m_client_hash,
                add_component_request.entity_id, 
                add_component_request.component_id, 
                add_component_request.component_payload);

            Message::WorkerDefaultResponse response = { result };
            {
                _response_payload(response);
            }

            break;
        }
        case RequestType::WorkerRemoveComponent:
        {
            Message::WorkerRemoveComponentRequest remove_component_request;
            {
                _request_payload(remove_component_request);
            }

            bool result = m_bridge.OnWorkerRemoveComponent(
                m_client_hash,
                remove_component_request.entity_id,
                remove_component_request.component_id);

            Message::WorkerDefaultResponse response = { result };
            {
                _response_payload(response);
            }

            break;
        }
        case RequestType::WorkerComponentUpdate:
        {
            Message::WorkerComponentUpdateRequest component_update_request;
            {
                _request_payload(component_update_request);
            }

            bool result = m_bridge.OnWorkerComponentUpdate(
                m_client_hash,
                component_update_request.entity_id,
                component_update_request.component_id, 
                component_update_request.component_payload, 
                component_update_request.entity_world_position);

            Message::WorkerDefaultResponse response = { result };
            {
                _response_payload(response);
            }

            break;
        }
        case RequestType::WorkerComponentQuery:
        {
            Message::WorkerComponentQueryRequest component_query_request;
            {
                _request_payload(component_query_request);
            }

#if 0
            bool result = m_bridge.OnWorkerComponentQuery(
                m_client_hash,
                component_update_request.entity_id,
                component_update_request.component_id,
                component_update_request.component_payload,
                component_update_request.entity_world_position);
#endif
            bool result = false;

            Message::WorkerDefaultResponse response = { result };
            {
                _response_payload(response);
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