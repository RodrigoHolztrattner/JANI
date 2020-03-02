////////////////////////////////////////////////////////////////////////////////
// Filename: JaniBridge.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniBridge.h"
#include "JaniRuntime.h"
#include "JaniWorkerInstance.h"

Jani::Bridge::Bridge(
    Runtime& _runtime,
    LayerId  _layer_id) :
    m_runtime(_runtime), 
    m_layer_id(_layer_id)
{
}

Jani::Bridge::~Bridge()
{
}

std::optional<Jani::WorkerInstance*> Jani::Bridge::TryAllocateNewWorker(
    LayerId                  _layer_id,
    Connection<>::ClientHash _client_hash,
    bool                     _is_user)
{
    // Check if there is space for more workers on this layer
    if (m_load_balance_strategy.maximum_workers && m_worker_instances.size() >= m_load_balance_strategy.maximum_workers.value())
    {
        return std::nullopt;
    }
    
    // Check other layer requirements, as necessary
    // ...

    auto worker_instance = std::make_unique<WorkerInstance>(
        *this, 
        _layer_id,
        _client_hash,
        _is_user);

    auto worker_instance_iter = m_worker_instances.insert({ _client_hash, std::move(worker_instance) });

    return worker_instance_iter.first->second.get();
}

bool Jani::Bridge::DisconnectWorker(Connection<>::ClientHash _client_hash)
{
    auto worker_instance_iter = m_worker_instances.find(_client_hash);
    if (worker_instance_iter != m_worker_instances.end())
    {
        m_worker_instances.erase(worker_instance_iter);

        return true;
    }

    return false;
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
    // Process worker messages
    // ...

    // Request database updates
    // ...
}

const std::unordered_map<Jani::WorkerId, std::unique_ptr<Jani::WorkerInstance>>& Jani::Bridge::GetWorkers() const
{
    return m_worker_instances;
}

std::unordered_map<Jani::WorkerId, std::unique_ptr<Jani::WorkerInstance>>& Jani::Bridge::GetWorkersMutable()
{
    return m_worker_instances;
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
    return m_worker_instances.size();
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

bool Jani::Bridge::OnWorkerLogMessage(
    WorkerInstance& _worker_instance,
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

std::optional<Jani::EntityId> Jani::Bridge::OnWorkerReserveEntityIdRange(
    WorkerInstance& _worker_instance,
    WorkerId        _worker_id, 
    uint32_t        _total_ids)
{
    return m_runtime.OnWorkerReserveEntityIdRange(
        _worker_instance, 
        _worker_id,
        _total_ids);
}

bool Jani::Bridge::OnWorkerAddEntity(
    WorkerInstance&      _worker_instance,
    WorkerId             _worker_id, 
    EntityId             _entity_id, 
    const EntityPayload& _entity_payload)
{
    return m_runtime.OnWorkerAddEntity(
        _worker_instance, 
        _worker_id, 
        _entity_id,
        _entity_payload);
}

bool Jani::Bridge::OnWorkerRemoveEntity(
    WorkerInstance& _worker_instance,
    WorkerId        _worker_id, 
    EntityId        _entity_id)
{
    return m_runtime.OnWorkerRemoveEntity(
        _worker_instance,
        _worker_id,
        _entity_id);
}

bool Jani::Bridge::OnWorkerAddComponent(
    WorkerInstance&         _worker_instance,
    WorkerId                _worker_id, 
    EntityId                _entity_id, 
    ComponentId             _component_id, 
    const ComponentPayload& _component_payload)
{
    return m_runtime.OnWorkerAddComponent(
        _worker_instance,
        _worker_id, 
        _entity_id, 
        _component_id,
        _component_payload);
}

bool Jani::Bridge::OnWorkerRemoveComponent(
    WorkerInstance& _worker_instance,
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

bool Jani::Bridge::OnWorkerComponentUpdate(
    WorkerInstance&              _worker_instance,
    WorkerId                     _worker_id,
    EntityId                     _entity_id,
    ComponentId                  _component_id,
    const ComponentPayload&      _component_payload,
    std::optional<WorldPosition> _entity_world_position)
{
    return m_runtime.OnWorkerComponentUpdate(
        _worker_instance,
        _worker_id, 
        _entity_id, 
        _component_id, 
        _component_payload, 
        _entity_world_position);
}

Jani::WorkerRequestResult Jani::Bridge::OnWorkerComponentQuery(
    WorkerInstance&       _worker_instance,
    WorkerId              _worker_id,
    EntityId              _entity_id,
    const ComponentQuery& _component_query)
{
    return m_runtime.OnWorkerComponentQuery(
        _worker_instance,
        _worker_id, 
        _entity_id, 
        _component_query);
}