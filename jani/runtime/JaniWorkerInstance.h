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
    * Returns if this worker is over its entity capacity
    */
    bool IsOverCapacity() const;

    /*
    * Returns 4 optional entity ids that are on the limits of this worker area of influence
    */
    EntitiesOnAreaLimit GetEntitiesOnAreaLimit() const;

    /*
    * Returns the world rect this worker currently have
    */
    WorldRect GetWorldRect() const;

    /*
    * This function will process a request from the counterpart worker and attempt to resolve it, returning
    * a response whenever applicable
    */
    void ProcessRequest(const Request& _request, cereal::BinaryInputArchive& _request_payload, cereal::BinaryOutputArchive& _response_payload);

protected:

    /*
    * Resets the over capacity flag
    */
    void ResetOverCapacityFlag();

////////////////////////
private: // VARIABLES //
////////////////////////

    Bridge&                  m_bridge;
    LayerId                  m_layer_id;
    Connection<>::ClientHash m_client_hash;
    bool                     m_is_user;
    bool                     m_use_spatial_area    = false;
    WorldRect                m_area;
    bool                     m_is_over_capacity    = false;
    int32_t                  m_total_over_capacity = 0;

    EntitiesOnAreaLimit m_entities_on_area_limit;
    // std::unordered_map<EntityId, uint32_t> m_entities_component_count;
};

// Jani
JaniNamespaceEnd(Jani)