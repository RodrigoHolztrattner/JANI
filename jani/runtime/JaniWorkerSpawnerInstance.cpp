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

bool Jani::WorkerSpawnerInstance::RequestWorkerForLayer(LayerId _layer_id, uint32_t _timeout_ms)
{
    if (m_connection && (!m_worker_spawn_status[_layer_id] || m_worker_spawn_status[_layer_id]->has_timed_out))
    {
        Message::WorkerSpawnRequest worker_spawn_request;
        worker_spawn_request.runtime_worker_connection_port = m_runtime_worker_connection_port;
        worker_spawn_request.runtime_ip                     = m_runtime_address;
        worker_spawn_request.layer_id                       = _layer_id;

        bool result = m_request_manager.MakeRequest(
            *m_connection,
            RequestType::SpawnWorkerForLayer,
            worker_spawn_request);
        if (result)
        {
            WorkerSpawnStatus worker_spawn_status;
            worker_spawn_status.timeout_time_ms = _timeout_ms;
            m_worker_spawn_status[_layer_id] = std::move(worker_spawn_status);

            return true;
        }
    }

    return false;
}

void Jani::WorkerSpawnerInstance::AcknowledgeWorkerSpawn(LayerId _layer_id)
{
    m_worker_spawn_status[_layer_id] = std::nullopt;
}

bool Jani::WorkerSpawnerInstance::IsExpectingWorkerForLayer(LayerId _layer_id) const
{
    assert(_layer_id < MaximumLayers);

    if (m_worker_spawn_status[_layer_id] && !m_worker_spawn_status[_layer_id]->has_timed_out)
    {
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - m_worker_spawn_status[_layer_id]->request_time).count();
        if (elapsed_time > m_worker_spawn_status[_layer_id]->timeout_time_ms)
        {
            m_worker_spawn_status[_layer_id]->has_timed_out = true;
        }
    }

    if (!m_worker_spawn_status[_layer_id] || m_worker_spawn_status[_layer_id]->has_timed_out)
    {
        return false;
    }

    return true;
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