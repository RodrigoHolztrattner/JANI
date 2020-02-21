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

////////////////////////
private: // VARIABLES //
////////////////////////

    Runtime& m_runtime;

    uint32_t m_layer_id = std::numeric_limits<uint32_t>::max();

    std::unique_ptr<Connection>              m_worker_connection;
    std::vector<std::unique_ptr<Connection>> m_client_connections;

};

// Jani
JaniNamespaceEnd(Jani)