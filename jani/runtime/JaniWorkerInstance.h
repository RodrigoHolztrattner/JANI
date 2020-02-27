////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorkerInstance.h
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
// Class name: WorkerInstance
////////////////////////////////////////////////////////////////////////////////
class WorkerInstance
{
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    WorkerInstance(
        Bridge&                  _bridge,
        LayerHash                _layer_hash,
        Connection<>::ClientHash _client_hash,
        bool                     _is_user);
    ~WorkerInstance();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * This function will process a request from the counterpart worker and attempt to resolve it, returning
    * a response whenever applicable
    */
    void ProcessRequest(const Request& _request, cereal::BinaryInputArchive& _request_payload, cereal::BinaryOutputArchive& _response_payload);

////////////////////////
private: // VARIABLES //
////////////////////////

    Bridge&                  m_bridge;
    LayerHash                m_layer_hash;
    Connection<>::ClientHash m_client_hash;
    bool                     m_is_user;
};

// Jani
JaniNamespaceEnd(Jani)