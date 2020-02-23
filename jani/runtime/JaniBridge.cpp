////////////////////////////////////////////////////////////////////////////////
// Filename: JaniBridge.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniBridge.h"
#include "JaniRuntime.h"

Jani::Bridge::Bridge(
    Runtime&                    _runtime,
    uint32_t                    _layer_id, 
    std::unique_ptr<Connection> _worker_connection) :
    m_runtime(_runtime), 
    m_layer_id(_layer_id), 
    m_worker_connection(std::move(_worker_connection))
{
    assert(m_worker_connection);


}

Jani::Bridge::~Bridge()
{
}

const std::string& Jani::Bridge::GetLayerName() const
{
    return m_layer_name;
}

Jani::Hash Jani::Bridge::GetLayerNameHash() const
{
    return m_layer_hash;
}

uint32_t Jani::Bridge::GetLayerId() const
{
    return m_layer_id;
}

const Jani::LayerLoadBalanceStrategy& Jani::Bridge::GetLoadBalanceStrategy() const
{
    return m_load_balance_strategy;
}

Jani::LayerLoadBalanceStrategyTypeFlags Jani::Bridge::GetLoadBalanceStrategyFlags() const
{
    return m_load_balance_strategy_flags;
}

void Jani::Bridge::Update()
{
    // Update the worker connections
    m_worker_connections;
    // m_client_connections; 

    // Process runtime messages
    // ...

    // Process client messages
    // ...

    // Process worker messages
    // ...

    // Request database updates
    // ...
}

uint32_t Jani::Bridge::GetLayerId() const
{
    return m_layer_id;
}

bool Jani::Bridge::IsValid() const
{
    // Check timeout on m_worker_connection
    // ...

    return true;
}

bool Jani::Bridge::AcceptLoadBalanceStrategy(LayerLoadBalanceStrategyBits _load_balance_strategy) const
{
    return m_load_balance_strategy_flags & _load_balance_strategy;
}

uint32_t Jani::Bridge::GetTotalWorkerCount() const
{
    return m_worker_connections.size();
}

bool Jani::Bridge::HasInterestOnComponent(ComponentId _component_id) const
{
    return m_component_interests[_component_id];
}

#if 0
uint32_t Jani::Bridge::GetDistanceToPosition(WorldPosition _position) const
{
    if (m_load_balance_strategy_flags & m_load_balance_strategy_flags && m_world_rect)
    {
        return m_world_rect->ManhattanDistanceFromPosition(_position);
    }

    return std::numeric_limits<uint32_t>::max();
}

uint32_t Jani::Bridge::GetWorldArea() const
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

Jani::WorkerRequestResult Jani::Bridge::OnWorkerConnect(WorkerId _worker_id)
{
    return WorkerRequestResult(true);
}

Jani::WorkerRequestResult Jani::Bridge::OnWorkerLogMessage(
    WorkerId       _worker_id, 
    WorkerLogLevel _log_level, 
    std::string    _log_title, 
    std::string    _log_message)
{
    return m_runtime.OnWorkerLogMessage(_worker_id, _log_level, _log_title, _log_message);
}

Jani::WorkerRequestResult Jani::Bridge::OnWorkerReserveEntityIdRange(
    WorkerId _worker_id, 
    uint32_t _total_ids)
{
    return m_runtime.OnWorkerReserveEntityIdRange(_worker_id, _total_ids);
}

Jani::WorkerRequestResult Jani::Bridge::OnWorkerAddEntity(
    WorkerId             _worker_id, 
    EntityId             _entity_id, 
    const EntityPayload& _entity_payload)
{
    return m_runtime.OnWorkerAddEntity(_worker_id, _entity_id, _entity_payload);
}

Jani::WorkerRequestResult Jani::Bridge::OnWorkerRemoveEntity(
    WorkerId _worker_id, 
    EntityId _entity_id)
{
    return m_runtime.OnWorkerRemoveEntity(_worker_id, _entity_id);
}

Jani::WorkerRequestResult Jani::Bridge::OnWorkerAddComponent(
    WorkerId                _worker_id, 
    EntityId                _entity_id, 
    ComponentId             _component_id, 
    const ComponentPayload& _component_payload)
{
    return m_runtime.OnWorkerAddComponent(_worker_id, _entity_id, _component_id, _component_payload);
}

Jani::WorkerRequestResult Jani::Bridge::OnWorkerRemoveComponent(
    WorkerId _worker_id,
    EntityId _entity_id, 
    ComponentId _component_id)
{
    return m_runtime.OnWorkerRemoveComponent(_worker_id, _entity_id, _component_id);
}

Jani::WorkerRequestResult Jani::Bridge::OnWorkerComponentUpdate(
    WorkerId                     _worker_id,
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    const ComponentPayload&      _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    return m_runtime.OnWorkerComponentUpdate(_worker_id, _entity_id, _component_id, _component_payload, _entity_world_position);
}