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