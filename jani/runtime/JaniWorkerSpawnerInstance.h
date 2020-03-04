////////////////////////////////////////////////////////////////////////////////
// Filename: WorkerSpawnerInstance.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

////////////////////////////////////////////////////////////////////////////////
// Class name: WorkerSpawnerInstance
////////////////////////////////////////////////////////////////////////////////
class WorkerSpawnerInstance
{
    struct WorkerSpawnStatus
    {
        mutable bool                                       has_timed_out   = false;
        uint32_t                                           timeout_time_ms = std::numeric_limits<uint32_t>::max();
        std::chrono::time_point<std::chrono::steady_clock> request_time    = std::chrono::steady_clock::now();
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    WorkerSpawnerInstance();
    ~WorkerSpawnerInstance();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize this worker spawner instance, creating the connection with the physical worker spawner
    */
    bool Initialize(
        uint32_t    _local_port, 
        uint32_t    _spawner_port,
        std::string _spawner_address, 
        uint32_t    _runtime_worker_connection_port, 
        std::string _runtime_addres);

    /*
    * Request that the connected worker spawner spawns a worker instance of the given layer for the
    * runtime
    * The worker will attempt to connect through the default worker connection on the runtime object
    * Do not call this function withing small intervals, ideally this should be called every 10-30
    * seconds (or more)
    */
    bool RequestWorkerForLayer(LayerId _layer_id, uint32_t _timeout_ms = 10000);

    /*
    * Acknowledge a worker spawn for the given layer, this is required to determine if a request succeed
    * or if it timed out
    */
    void AcknowledgeWorkerSpawn(LayerId _layer_id);

    /*
    * Return if this spawner is expecting a worker of the given layer type to be created soon
    */
    bool IsExpectingWorkerForLayer(LayerId _layer_id) const;

    /*
    * Perform the internal connection update
    */
    void Update();

    /*
    * Returns if the connection between this instance and the actual worker spawner has timed-out
    */
    bool DidTimeout() const;

    /*
    * Reset the connection with the worker spawner
    */
    void ResetConnection();

////////////////////////
private: // VARIABLES //
////////////////////////

    std::unique_ptr<Connection<>> m_connection;
    RequestManager                m_request_manager;
    uint32_t                      m_local_port                        = std::numeric_limits<uint32_t>::max();
    uint32_t                      m_spawner_port                      = std::numeric_limits<uint32_t>::max();
    std::string                   m_spawner_address;
    uint32_t                      m_runtime_worker_connection_port    = std::numeric_limits<uint32_t>::max();
    std::string                   m_runtime_address;

    std::array<std::optional<WorkerSpawnStatus>, MaximumLayers> m_worker_spawn_status;
};

// Jani
JaniNamespaceEnd(Jani)