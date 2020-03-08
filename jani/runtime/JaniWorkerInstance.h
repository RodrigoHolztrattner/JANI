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
    friend Runtime;

    struct EntitiesOnAreaLimit
    {
        std::optional<EntityId>  extreme_top_entity;
        std::optional<EntityId>  extreme_right_entity;
        std::optional<EntityId>  extreme_left_entity;
        std::optional<EntityId>  extreme_bottom_entity;
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    WorkerInstance(
        Bridge&                  _bridge,
        LayerId                  _layer_id,
        Connection<>::ClientHash _client_hash,
        bool                     _is_user);
    ~WorkerInstance();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Return the layer id associated with this worker instance
    */
    LayerId GetLayerId() const;

    /*
    * Return the worker id associated with this instance
    */
    WorkerId GetId() const;

    /*
    * Return the connection client hash (that identifies this worker instance on the runtime connection)
    */
    Connection<>::ClientHash GetConnectionClientHash() const;

    /*
    * Returns if this worker uses spatial area as a load balance strategy
    */
    bool UseSpatialArea() const;

    /*
    * Return if this is a user worker instance
    */
    bool IsUserInstance() const;

    /*
    * Return a pair containing the network traffic for this worker (received/sent)
    */
    std::pair<uint64_t, uint64_t> GetNetworkTrafficPerSecond() const;

    /*
    * This function will process a request from the counterpart worker and attempt to resolve it, returning
    * a response whenever applicable
    */
    void ProcessRequest(const RequestInfo& _request, const RequestPayload& _request_payload, ResponsePayload& _response_payload);

////////////////////////
private: // VARIABLES //
////////////////////////

    Bridge&                  m_bridge;
    LayerId                  m_layer_id;
    Connection<>::ClientHash m_client_hash;
    bool                     m_is_user;
    bool                     m_use_spatial_area    = false;

    uint64_t m_total_data_received_per_second = 0;
    uint64_t m_total_data_sent_per_second     = 0;
};

// Jani
JaniNamespaceEnd(Jani)