////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorkerSpawnerCollection.h
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
// Class name: WorkerSpawnerCollection
////////////////////////////////////////////////////////////////////////////////
class WorkerSpawnerCollection
{
    struct WorkerSpawnerInfo
    {
        std::string ip;
        uint32_t    port = std::numeric_limits<uint32_t>::max();
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    WorkerSpawnerCollection();
    ~WorkerSpawnerCollection();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize this collection by reading the json file indicated by the given path
    */
    bool Initialize(const std::string& _config_file_path);

    /*
    * Returns if this collection was correctly initialized, the returned result is the same as the
    * one returned by the Initialize() function
    */
    bool IsValid() const;

    /*
    * Return a vector with all worker spawners infos
    */
    const std::vector<WorkerSpawnerInfo>& GetWorkerSpawnersInfos() const;

////////////////////////
private: // VARIABLES //
////////////////////////

    bool                           m_is_valid = false;
    std::vector<WorkerSpawnerInfo> m_worker_spawners;
};

// Jani
JaniNamespaceEnd(Jani)