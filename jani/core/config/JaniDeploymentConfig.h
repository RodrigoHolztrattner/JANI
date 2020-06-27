////////////////////////////////////////////////////////////////////////////////
// Filename: JaniDeploymentConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "../JaniInternal.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

////////////////////////////////////////////////////////////////////////////////
// Class name: DeploymentConfig
////////////////////////////////////////////////////////////////////////////////
class DeploymentConfig
{

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    DeploymentConfig();
    ~DeploymentConfig();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize this config class by reading the json file indicated by the given path
    */
    bool Initialize(const std::string& _config_file_path);

    /*
    * Returns if this config class was correctly initialized, the returned result is the same as the
    * one returned by the Initialize() function
    */
    bool IsValid() const;

    /*
    * Return the maximum world length
    */
    uint32_t GetMaximumWorldLength() const;

    /*
    * Return the worker length
    */
    uint32_t GetWorkerLength() const;

    /*
    * Return the number of grids per world line
    */
    uint32_t GetTotalGridsPerWorldLine() const;

    /*
    * Return the maximum number of workers that can co-exist according with the world dimensions
    */
    uint32_t GetMaximumWorkerCount() const;

    /*
    * Returns if the deployment uses a centralized world origin
    * If this is true it means that the world area starts at x:0 y:0 but the
    * minimum and maximum sizes are xmin:-m_maximum_world_length/2 ymin:-m_maximum_world_length/2
    * xmax:m_maximum_world_length/2 ymax:m_maximum_world_length/2
    * If false the world starts at x:0 y:0 and can go up to xmax:m_maximum_world_length
    * ymax:m_maximum_world_length (only positive regions)
    */
    bool UsesCentralizedWorldOrigin() const;

    /*
    * Return the runtime ip
    */
    const std::string& GetRuntimeIp() const;

    /*
    * Return the client worker listen port used by the runtime
    */
    uint32_t GetClientWorkerListenPort() const;

    /*
    * Return the server worker listen port used by the runtime
    */
    uint32_t GetServerWorkerListenPort() const;

    /*
    * Return the inspector listen port used by the runtime
    */
    uint32_t GetInspectorListenPort() const;

    /*
    * Return the number of threads that should be used in the runtime thread pool 
    * Special cases:
    * 
    *       0 = only the main thread
    *      -1 = up to the maximum number of cores
    *
    */
    uint32_t GetThreadPoolSize() const;

////////////////////////
private: // VARIABLES //
////////////////////////

    std::string m_runtime_ip;
    uint32_t    m_client_worker_listen_port = 0;
    uint32_t    m_server_worker_listen_port = 0;
    uint32_t    m_inspector_listen_port     = 0;

    int32_t m_thread_pool_size = 0;

    bool     m_is_valid                      = false;
    bool     m_uses_centralized_world_origin = true;
    uint32_t m_maximum_world_length          = 0;
    uint32_t m_worker_length                 = 0;
    uint32_t m_grids_per_world_line          = 0;
};

// Jani
JaniNamespaceEnd(Jani)