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
// Class name: LoadBalancer
////////////////////////////////////////////////////////////////////////////////
class LoadBalancer
{

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    LoadBalancer();
    ~LoadBalancer();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Search the input set for a bridge that can accept a new user on the selected layer
    * This search will take in consideration the layer load balance strategies matched
    * against the user values
    * Returning a nullopt has 2 meanings: Or there is no valid bridge for the specified
    * layer or there are but they do not match the load balance requirements for this 
    * layer, in both occasions a new bridge should be created
    * It's important to mention that, if a layer has a limit on the number of workers
    * and this limit was already achieved, any other load balancing rules will be 
    * ignored since it's impossible to create another worker, in this case the return
    * value will be the "best" possible given the case
    */
    std::optional<Bridge*> TryFindValidBridgeForLayer(
        const std::map<Hash, std::unique_ptr<LayerBridgeSet>>& _bridge_sets, 
        const User&                                            _triggering_user, 
        Hash                                                   _layer_name_hash) const;

};

// Jani
JaniNamespaceEnd(Jani)