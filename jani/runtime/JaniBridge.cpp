////////////////////////////////////////////////////////////////////////////////
// Filename: JaniBridge.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniBridge.h"

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

void Jani::Bridge::Update()
{
    // Update the worker connection and all client connections
    m_worker_connection; 
    m_client_connections; 

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

bool Jani::Bridge::AcceptLoadBalanceStrategy(LayerLoadBalanceStrategyType _load_balance_strategy) const
{
    return m_active_load_balance_strategy_bits & _load_balance_strategy;
}

uint32_t Jani::Bridge::GetTotalUserCount() const
{
    return m_client_connections.size();
}

uint32_t Jani::Bridge::GetDistanceToPosition(WorldPosition _position) const
{
    if (m_active_load_balance_strategy_bits & m_active_load_balance_strategy_bits)
    {
        return 0;
    }

    return std::numeric_limits<uint32_t>::max();
}