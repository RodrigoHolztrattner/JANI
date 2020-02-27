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

class Runtime;
class WorkerInstance;

/*

:: Bridge:
    - Cada worker possui uma conexao direta com o Runtime, vamos chamar a classe responsavel por gerenciar essa conexao de Bridge
    - Essa Bridge possui conexao direta com players
    - A bridge deve ser classificada de acordo com o layer do worker ao qual ela esta conectada
    - Ela deve receber dados dos players e dar forwarding para os workers relacionados
    - Ela deve receber atualizacoes dos workers e dar forwarding para os players relacionados
    - Ela deve fazer requisicoes de escrita na Database sempre que algum componente for modificado
    - Caso o worker conectado a ela caia, ela deve avisar o Runtime para que um novo worker seja criado no lugar
    - Caso um jogador perca a conexao com a bridge, ele eh desconectado

*/

////////////////////////////////////////////////////////////////////////////////
// Class name: Bridge
////////////////////////////////////////////////////////////////////////////////
class Bridge
{
    friend WorkerInstance;

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Bridge(
        Runtime& _runtime,
        uint32_t _layer_id);
    ~Bridge();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Attempt to allocate a new worker for the given layer
    * This operation is susceptible to the feasibility of creating a new one according
    * to the limits imposed by the layer itself
    */
    std::optional<WorkerInstance*> TryAllocateNewWorker(
        LayerHash                _layer_hash,
        Connection<>::ClientHash _client_hash,
        bool                     _is_user);

    /*
    * Return the assigned layer name
    */
    const std::string& GetLayerName() const;

    /*
    * Return the assigned layer name hash
    */
    Hash GetLayerNameHash() const;

    /*
    * Return the assigned layer ID
    */
    uint32_t GetLayerId() const;

    /*
    * Return the load balance strategy for this bridge
    */
    const LayerLoadBalanceStrategy& GetLoadBalanceStrategy() const;

    /*
    * Return the load balance strategy flags for this bridge
    */
    LayerLoadBalanceStrategyTypeFlags GetLoadBalanceStrategyFlags() const;

    /*
    * Returns if a worker for this bridge accepts the given load balance
    * strategy
    */
    bool AcceptLoadBalanceStrategy(LayerLoadBalanceStrategyBits _load_balance_strategy) const;

    /*
    * Return the total number of workers connected through this bridge
    */
    uint32_t GetTotalWorkerCount() const;

    /*
    * Return if this bridge is valid and able to operate
    * Returning false will cause the runtime to shutdown this bridge and create a new one
    */
    bool IsValid() const;

#if 0
    /*
    * Returns the Manhattan distance from a worker influence location to the
    * given position
    * Calling this function is only valid if this bridge layer support spatial location
    * as one of its load balance strategies
    * Zero means that the given position is inside the area of influence
    */
    uint32_t GetDistanceToPosition(WorldPosition _position) const;

    /*
    * Returns the world area a worker is affecting
    * Calling this function is only valid if this bridge layer support spatial location
    * as one of its load balance strategies
    */
    uint32_t GetWorldArea() const;
#endif

public:

    /*
    *
    */
    void Update();

/////////////////////////////////////////////
private: // WORKER -> BRIDGE COMMUNICATION //
/////////////////////////////////////////////

    /*
    * Received when a worker successfully connects to this bridge
    */
    WorkerRequestResult OnWorkerConnect(WorkerId _worker_id);

    /*
    * Received when a worker sends a log message
    */
    bool OnWorkerLogMessage(
        WorkerId       _worker_id, 
        WorkerLogLevel _log_level, 
        std::string    _log_title, 
        std::string    _log_message);

    /*
    * Received when a worker sends a request to reserve a range of entity ids
    */
    std::optional<Jani::EntityId> OnWorkerReserveEntityIdRange(
        WorkerId _worker_id, 
        uint32_t _total_ids);

    /*
    * Received when a worker requests to add a new entity
    */
    bool OnWorkerAddEntity(
        WorkerId             _worker_id, 
        EntityId             _entity_id, 
        const EntityPayload& _entity_payload);

    /*
    * Received when a worker requests to remove an existing entity
    */
    bool OnWorkerRemoveEntity(
        WorkerId _worker_id, 
        EntityId _entity_id);

    /*
    * Received when a worker requests to add a new component for the given entity
    */
    bool OnWorkerAddComponent(
        WorkerId                _worker_id, 
        EntityId                _entity_id, 
        ComponentId             _component_id, 
        const ComponentPayload& _component_payload);

    /*
    * Received when a worker requests to remove an existing component for the given entity
    */
    bool OnWorkerRemoveComponent(
        WorkerId _worker_id,
        EntityId _entity_id, 
        ComponentId _component_id);

    /*
    * Received when a worker updates a component
    * Can only be received if this worker has write authority over the indicated entity
    * If this component changes the entity world position, it will generate an entity position change event over the runtime
    */
    bool OnWorkerComponentUpdate(
        WorkerId                     _worker_id, 
        EntityId                     _entity_id, 
        ComponentId                  _component_id, 
        const ComponentPayload&      _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

    /*
    * Received when a worker request a component query 
    */
    WorkerRequestResult OnWorkerComponentQuery(
        WorkerId              _worker_id,
        EntityId              _entity_id,
        const ComponentQuery& _component_query);


    //         std::optional<bool>          _authority_loss_imminent_acknowledgement, 

/////////////////////////////////////////////
private: // BRIDGE -> WORKER COMMUNICATION //
/////////////////////////////////////////////

    WorkerRequestResult BroadcastComponentAdded(
        WorkerId                     _worker_id,
        EntityId                     _entity_id,
        ComponentId                  _component_id,
        const ComponentPayload& _component_payload,
        std::optional<WorldPosition> _entity_world_position);

                // bridge->BroadcastComponentAdded(_worker_id, _entity_id, _component_id, _component_payload);
                        // bridge->BroadcastComponentRemoved(_worker_id, _entity_id, _component_id, _component_payload);
                        // bridge->BroadcastComponentUpdated(_worker_id, _entity_id, _component_id, _component_payload);

////////////////////////
private: // VARIABLES //
////////////////////////

    Runtime& m_runtime;

    std::string m_layer_name;
    Hash        m_layer_hash;
    uint32_t    m_layer_id = std::numeric_limits<uint32_t>::max();

    LayerLoadBalanceStrategy          m_load_balance_strategy;
    LayerLoadBalanceStrategyTypeFlags m_load_balance_strategy_flags;

    std::vector<std::unique_ptr<WorkerInstance>> m_worker_instances;
};

// Jani
JaniNamespaceEnd(Jani)