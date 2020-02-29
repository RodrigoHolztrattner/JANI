////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorkerSpawnerInstance.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorkerSpawnerInstance.h"

Jani::WorkerSpawnerInstance::WorkerSpawnerInstance()
{
}

Jani::WorkerSpawnerInstance::~WorkerSpawnerInstance()
{
}

bool Jani::WorkerSpawnerInstance::Initialize(
    uint32_t    _local_port,
    uint32_t    _spawner_port,
    std::string _spawner_address,
    uint32_t    _runtime_worker_connection_port,
    std::string _runtime_addres)
{
    m_local_port                     = _local_port;
    m_spawner_port                   = _spawner_port;
    m_spawner_address                = std::move(_spawner_address);
    m_runtime_worker_connection_port = _runtime_worker_connection_port;
    m_runtime_address                = std::move(_runtime_addres);

    ResetConnection();

    return true;
}

bool Jani::WorkerSpawnerInstance::RequestWorkerForLayer(LayerHash _layer_hash)
{
    if (m_connection)
    {
        Message::WorkerSpawnRequest worker_spawn_request;
        worker_spawn_request.runtime_worker_connection_port = m_runtime_worker_connection_port;
        worker_spawn_request.runtime_ip                     = m_runtime_address;
        worker_spawn_request.layer_hash                     = _layer_hash;

        bool result = m_request_manager.MakeRequest(
            *m_connection,
            RequestType::SpawnWorkerForLayer,
            worker_spawn_request);

        if (result)
        {
            // Set that we are waiting for a new worker on this layer
            // ...

            return true;
        }
    }

    return false;
}

void Jani::WorkerSpawnerInstance::Update()
{
    if (m_connection)
    {
        m_connection->Update();

        m_request_manager.Update(
            *m_connection, 
            [](auto _client_hash, const Request& _request, const RequestResponse& _response)
            {
                switch (_request.type)
                {
                    case RequestType::SpawnWorkerForLayer:
                    {
                        auto worker_spawn_response = _response.GetResponse<Message::WorkerSpawnResponse>();
                        
                        // Bla
                        // ...

                        break;
                    }
                }
            });
    }
}

bool Jani::WorkerSpawnerInstance::DidTimeout() const
{
    bool timed_out = false;
    if (m_connection)
    {
        m_connection->DidTimeout(
            [&](auto _client_hash)
            {
                timed_out = true;
            });

        return timed_out;
    }

    return true;
}

void Jani::WorkerSpawnerInstance::ResetConnection()
{
    m_connection = std::make_unique<Connection<>>(
        m_local_port,
        m_spawner_port,
        m_spawner_address.c_str());
}