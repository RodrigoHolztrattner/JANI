////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include <cstdint>
#include <bitset>
#include <entityx/entityx.h>

/////////////
// DEFINES //
/////////////

#define JaniNamespaceBegin(name)                  namespace name {
#define JaniNamespaceEnd(name)                    }

// Set all members from the current struct/class as serializable
#define JaniSerializable()                                                  \
template <class Archive>                                                    \
void serialize(Archive& ar)                                                 \
{                                                                           \
    boost::pfr::for_each_field(*this, [&ar](auto& field, std::size_t idx)   \
    {                                                                       \
        ar(field);                                                          \
    });                                                                     \
}

#define DISABLE_COPY(class_name) class_name(const class_name&) = delete;\
                                 class_name& operator=(const class_name&) = delete

namespace Jani
{

class Bridge;
class Runtime;
class Database;
class WorkerSpawnerInstance;
class WorkerInstance; 
class ServerEntity;
class ClientEntity;
using Entity = ClientEntity;
class EntityManager;

struct WorldPosition;
struct WorldArea;
struct WorldRect;

using WorkerId      = uint64_t;
using ComponentId   = uint64_t;
using EntityId      = uint64_t;
using LocalEntityId = decltype(std::declval<entityx::Entity::Id>().index());

static const uint32_t MaximumLayers           = 32;
static const uint32_t MaximumEntityComponents = 64;
using                 ComponentMask           = std::bitset<MaximumEntityComponents>;

template <typename C, typename EM>
using ComponentHandle = entityx::ComponentHandle<C, EM>;

static const uint32_t MaximumPacketSize = 4096;

using Hash      = uint64_t;
using LayerId   = uint64_t;

/*
    => Pergunta: Ao adicionar uma entidade, ela deve ser adicionada diretamente no servidor
    ou isso deve acontecer apenas localmente?

        - O motivo dessa pergunta eh porque se eu nunca adicionar componentes que deram ser
        replicados, nao existe motivo do servidor saber dessa entidade, afinal ela nao sera salva
        e nem repassada para outros workers

        - Talvez eu deva adicionar a entidade localmente e apenas replicar caso um desses
        componentes seja adicionado?

        - Se eu adicionar a entidade localmente, devo usar um dos IDs reservados?

    -----------------------------------------------------------------------------------------------------

    - Preciso de um map entre ComponentId e Component class hash, assim como o tamanho dele e a entrada na
    component mask?!
    - Preciso garantir que componentes replicados sejam POD
    - Preciso criar um entity manager que permita:
        - Criar entidades
    - Preciso de um system manager que permita:
        - Iterar entre um set de entidades que possuam tal componente

    - A class Entity deve permitir:
        - Adicionar componentes
        - Remover componentes
        - Update de componentes
        - Verificar componentes
        - For each component
        - Component mask

*/

} // Jani

#undef max
#undef min