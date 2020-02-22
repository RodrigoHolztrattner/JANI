////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.h
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

class Database;
class Bridge;
class LayerCollection;
class WorkerSpawnerCollection;

////////////////////////////////////////////////////////////////////////////////
// Class name: Runtime
////////////////////////////////////////////////////////////////////////////////
class Runtime
{
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Runtime(Database& _database);
    ~Runtime();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    
    */
    bool Initialize(
        std::unique_ptr<LayerCollection>         layer_collection, 
        std::unique_ptr<WorkerSpawnerCollection> worker_spawner_collection);

    /*
    *
    */
    void Update();

private:

    /*
    *
    */
    void ReceiveIncommingUserConnections();

    /*
    *
    */
    void ApplyLoadBalanceRequirements();

////////////////////////
private: // VARIABLES //
////////////////////////

    Database& m_database;

    std::unique_ptr<LayerCollection>         m_layer_collection;
    std::unique_ptr<WorkerSpawnerCollection> m_worker_spawner_collection;

    std::map<Hash, std::unique_ptr<LayerBridgeSet>> m_layer_bridge_sets;
};

// Jani
JaniNamespaceEnd(Jani)