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
    *
    */
    void Update();

    /*
    * Return the assigned layer ID
    */
    uint32_t GetLayerId() const;

    /*
    * Return if this bridge is valid and able to operate
    * Returning false will cause the runtime to shutdown this bridge and create a new one
    */
    bool IsValid() const;

    /*
    * Returns if the underlying worker for this bridge accepts the given load balance
    * strategy
    */
    bool AcceptLoadBalanceStrategy(LayerLoadBalanceStrategyType _load_balance_strategy) const;

    /*
    * Return the total number of users connected through this bridge
    */
    uint32_t GetTotalUserCount() const;

    /*
    * Returns the Manhattan distance from the underlying worker influence location to the
    * given position
    * Calling this function is only valid if this bridge layer support spatial location
    * as one of its load balance strategies
    * Zero means that the given position is inside the area of influence
    */
    uint32_t GetDistanceToPosition(WorldPosition _position) const;

////////////////////////
private: // VARIABLES //
////////////////////////

    Runtime& m_runtime;

    LayerLoadBalanceStrategyTypeBits m_active_load_balance_strategy_bits;

    uint32_t m_layer_id = std::numeric_limits<uint32_t>::max();

    std::unique_ptr<Connection>              m_worker_connection;
    std::vector<std::unique_ptr<Connection>> m_client_connections;

};

// Jani
JaniNamespaceEnd(Jani)