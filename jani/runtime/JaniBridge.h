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
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Bridge(
        Runtime&                    _runtime,
        uint32_t                    _layer_id, 
        std::unique_ptr<Connection> _worker_connection);
    ~Bridge();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

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
    * Returns if the underlying worker for this bridge accepts the given load balance
    * strategy
    */
    bool AcceptLoadBalanceStrategy(LayerLoadBalanceStrategyBits _load_balance_strategy) const;

    /*
    * Return the total number of workers connected through this bridge
    */
    uint32_t GetTotalWorkerCount() const;

    /*
    * Returns if this bridge has interest on the given component type
    */
    bool HasInterestOnComponent(ComponentId _component_id) const;

    /*
    * Return if this bridge is valid and able to operate
    * Returning false will cause the runtime to shutdown this bridge and create a new one
    */
    bool IsValid() const;

#if 0
    /*
    * Returns the Manhattan distance from the underlying worker influence location to the
    * given position
    * Calling this function is only valid if this bridge layer support spatial location
    * as one of its load balance strategies
    * Zero means that the given position is inside the area of influence
    */
    uint32_t GetDistanceToPosition(WorldPosition _position) const;

    /*
    * Returns the world area the underlying worker is affecting
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

///////////////////////////////////
private: // WORKER COMMUNICATION //
///////////////////////////////////

    /*
    * Received when the underlying worker successfully connects to this bridge
    */
    WorkerRequestResult OnWorkerConnect(WorkerId _worker_id);

    /*
    * Received when the underlying worker send a log message
    */
    WorkerRequestResult OnWorkerLogMessage(
        WorkerId       _worker_id, 
        WorkerLogLevel _log_level, 
        std::string    _log_title, 
        std::string    _log_message);

    /*
    * Received when the underlying worker send a request to reserve a range of entity ids
    */
    WorkerRequestResult OnWorkerReserveEntityIdRange(
        WorkerId _worker_id, 
        uint32_t _total_ids);

    /*
    * Received when the underlying worker requests to add a new entity
    */
    WorkerRequestResult OnWorkerAddEntity(
        WorkerId             _worker_id, 
        EntityId             _entity_id, 
        const EntityPayload& _entity_payload);

    /*
    * Received when the underlying worker requests to remove an existing entity
    */
    WorkerRequestResult OnWorkerRemoveEntity(
        WorkerId _worker_id, 
        EntityId _entity_id);

    /*
    * Received when the underlying worker requests to add a new component for the given entity
    */
    WorkerRequestResult OnWorkerAddComponent(
        WorkerId                _worker_id, 
        EntityId                _entity_id, 
        ComponentId             _component_id, 
        const ComponentPayload& _component_payload);

    /*
    * Received when the underlying worker requests to remove an existing component for the given entity
    */
    WorkerRequestResult OnWorkerRemoveComponent(
        WorkerId _worker_id,
        EntityId _entity_id, 
        ComponentId _component_id);

    /*
    * Received when the underlying worker updates a component
    * Can only be received if this worker has write authority over the indicated entity
    * If this component changes the entity world position, it will generate an entity position change event over the runtime
    */
    WorkerRequestResult OnWorkerComponentUpdate(
        WorkerId                     _worker_id, 
        EntityId                     _entity_id, 
        ComponentId                  _component_id, 
        const ComponentPayload&      _component_payload, 
        std::optional<WorldPosition> _entity_world_position);

    //         std::optional<bool>          _authority_loss_imminent_acknowledgement, 

////////////////////////
private: // VARIABLES //
////////////////////////

    Runtime& m_runtime;

    std::string m_layer_name;
    Hash        m_layer_hash;
    uint32_t    m_layer_id = std::numeric_limits<uint32_t>::max();

    LayerLoadBalanceStrategy          m_load_balance_strategy;
    LayerLoadBalanceStrategyTypeFlags m_load_balance_strategy_flags;

    std::vector<std::unique_ptr<Connection>> m_worker_connections;
    // std::vector<std::unique_ptr<Connection>> m_client_connections; Not used because clients are going to be normal workers too

    std::array<bool, MaximumEntityComponents> m_component_interests;


    /*
        * Preciso de uma quadtree ou parecido para organizar onde as entidades controladas pelos workers
        estao localizadas, porque eu precisarei resolver queries do tipo:

            - Layer "game" tem interesse no layer "npc_ai"
            - Componente "ai_position" eh escrito para entidade id "947", runtime processa a request e ela eh aceita
            - Runtime pergunta para cada bridge se o layer da mesma tem interesse no componente "ai_position"
            - Bridge com layer "game" responde com "sim, escolhe eu pls! eu nunca te pedi nada!"
            - Bridge verifica se o seu layer possui uma area (spatial area), como ele tem ele precisa verificar antes 
            para quais workers a entidade dona do componente esta dentro da area de interesse
            - Bridge com layer "game" faz broadcast de component update do tipo "ai_position" para todos workers selecionados
            pela selecao anterior
            - Caso nao tivesse uma area (spatial area) a bridge simplesmente iria fazer broadcast

        Lembrando que em OnWorkerComponentUpdate() (e possivelmente em component add) eu ganho a informacao
        de onde a entity se encontra no momento!
    */
};

// Jani
JaniNamespaceEnd(Jani)