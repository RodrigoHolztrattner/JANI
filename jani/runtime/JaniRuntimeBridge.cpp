////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntimeBridge.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntimeBridge.h"
#include "JaniRuntime.h"
#include "JaniRuntimeWorkerReference.h"

Jani::RuntimeBridge::RuntimeBridge(
    Runtime& _runtime,
    LayerId  _layer_id) :
    m_runtime(_runtime), 
    m_layer_id(_layer_id)
{
}

Jani::RuntimeBridge::~RuntimeBridge()
{
}

bool Jani::RuntimeBridge::DisconnectWorker(Connection<>::ClientHash _client_hash)
{
    auto worker_instance_iter = m_worker_instances.find(_client_hash);
    if (worker_instance_iter != m_worker_instances.end())
    {
        m_worker_instances.erase(worker_instance_iter);

        return true;
    }

    return false;
}

const std::string& Jani::RuntimeBridge::GetLayerName() const
{
    return m_layer_name;
}

Jani::Hash Jani::RuntimeBridge::GetLayerNameHash() const
{
    return m_layer_hash;
}

uint32_t Jani::RuntimeBridge::GetLayerId() const
{
    return m_layer_id;
}

void Jani::RuntimeBridge::Update()
{
    // Process worker messages
    // ...

    // Request database updates
    // ...
}

const std::unordered_map<Jani::WorkerId, std::unique_ptr<Jani::RuntimeWorkerReference>>& Jani::RuntimeBridge::GetWorkers() const
{
    return m_worker_instances;
}

std::unordered_map<Jani::WorkerId, std::unique_ptr<Jani::RuntimeWorkerReference>>& Jani::RuntimeBridge::GetWorkersMutable()
{
    return m_worker_instances;
}

bool Jani::RuntimeBridge::IsValid() const
{
    // Check timeout on m_worker_connection
    // ...

    return true;
}

uint32_t Jani::RuntimeBridge::GetTotalWorkerCount() const
{
    return m_worker_instances.size();
}

#if 0
uint32_t Jani::RuntimeBridge::GetDistanceToPosition(WorldPosition _position) const
{
    if (m_load_balance_strategy_flags & m_load_balance_strategy_flags && m_world_rect)
    {
        return m_world_rect->ManhattanDistanceFromPosition(_position);
    }

    return std::numeric_limits<uint32_t>::max();
}

uint32_t Jani::RuntimeBridge::GetWorldArea() const
{
    if (m_load_balance_strategy_flags & m_load_balance_strategy_flags && m_world_rect)
    {
        return m_world_rect->width * m_world_rect->height;
    }

    return std::numeric_limits<uint32_t>::max();
}
#endif

//
//
//

bool Jani::RuntimeBridge::OnWorkerLogMessage(
    RuntimeWorkerReference& _worker_instance,
    WorkerId        _worker_id, 
    WorkerLogLevel  _log_level, 
    std::string     _log_title, 
    std::string     _log_message)
{
    return m_runtime.OnWorkerLogMessage(
        _worker_instance,
        _worker_id,
        _log_level,
        _log_title,
        _log_message);
}

std::optional<Jani::EntityId> Jani::RuntimeBridge::OnWorkerReserveEntityIdRange(
    RuntimeWorkerReference& _worker_instance,
    WorkerId        _worker_id, 
    uint32_t        _total_ids)
{
    return m_runtime.OnWorkerReserveEntityIdRange(
        _worker_instance, 
        _worker_id,
        _total_ids);
}

bool Jani::RuntimeBridge::OnWorkerAddEntity(
    RuntimeWorkerReference& _worker_instance,
    WorkerId                _worker_id, 
    EntityId                _entity_id, 
    EntityPayload           _entity_payload)
{
    return m_runtime.OnWorkerAddEntity(
        _worker_instance, 
        _worker_id, 
        _entity_id,
        std::move(_entity_payload));
}

bool Jani::RuntimeBridge::OnWorkerRemoveEntity(
    RuntimeWorkerReference& _worker_instance,
    WorkerId        _worker_id, 
    EntityId        _entity_id)
{
    return m_runtime.OnWorkerRemoveEntity(
        _worker_instance,
        _worker_id,
        _entity_id);
}

bool Jani::RuntimeBridge::OnWorkerAddComponent(
    RuntimeWorkerReference& _worker_instance,
    WorkerId                _worker_id, 
    EntityId                _entity_id, 
    ComponentId             _component_id, 
    ComponentPayload        _component_payload)
{
    return m_runtime.OnWorkerAddComponent(
        _worker_instance,
        _worker_id, 
        _entity_id, 
        _component_id,
        std::move(_component_payload));
}

bool Jani::RuntimeBridge::OnWorkerRemoveComponent(
    RuntimeWorkerReference& _worker_instance,
    WorkerId        _worker_id,
    EntityId        _entity_id, 
    ComponentId     _component_id)
{
    return m_runtime.OnWorkerRemoveComponent(
        _worker_instance,
        _worker_id,
        _entity_id,
        _component_id);
}

bool Jani::RuntimeBridge::OnWorkerComponentUpdate(
    RuntimeWorkerReference&      _worker_instance,
    WorkerId                     _worker_id,
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    ComponentPayload             _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    return m_runtime.OnWorkerComponentUpdate(
        _worker_instance,
        _worker_id, 
        _entity_id, 
        _component_id, 
        std::move(_component_payload),
        _entity_world_position);
}

bool Jani::RuntimeBridge::OnWorkerComponentInterestQueryUpdate(
    RuntimeWorkerReference&     _worker_instance,
    WorkerId                    _worker_id,
    EntityId                    _entity_id,
    ComponentId                 _component_id,
    std::vector<ComponentQuery> _component_queries)
{
    return m_runtime.OnWorkerComponentInterestQueryUpdate(
        _worker_instance,
        _worker_id,
        _entity_id,
        _component_id,
        std::move(_component_queries));
}