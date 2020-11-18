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
#include <ctti/detail/hash.hpp>

/////////////
// DEFINES //
/////////////

namespace Jani
{
    class RuntimeBridge;
    class Runtime;
    class RuntimeDatabase;
    class RuntimeWorkerSpawnerReference;
    class RuntimeWorkerReference;
    class ServerEntity;
    class ClientEntity;
    using Entity = ClientEntity;
    class EntityManager;
    class Snapshot;
    class SnapshotGenerator;

    struct WorldPosition;
    struct WorldPositionHasher;
    struct WorldPositionComparator;
    struct WorldArea;
    struct WorldRect;

    class DeploymentConfig;
    class LayerConfig;
    class WorkerSpawnerConfig;

    class Region;
    class RegionSaver;
    class RegionLoader;

    using WorldCellCoordinates           = WorldPosition;
    using WorldCellCoordinatesHasher     = WorldPositionHasher;
    using WorldCellCoordinatesComparator = WorldPositionComparator;

    using WorkerId      = uint64_t;
    using ComponentId   = uint64_t;
    using ComponentHash = uint64_t; // fixed at uint64_t for propper serialization, value should be the same as ctti::detail::hash_t;
    using EntityId      = uint64_t;
    using LocalEntityId = decltype(std::declval<entityx::Entity::Id>().index());

    static WorkerId      InvalidWorkerId      = std::numeric_limits<WorkerId>::max();
    static ComponentId   InvalidComponentId   = std::numeric_limits<ComponentId>::max();
    static ComponentHash InvalidComponentHash = std::numeric_limits<ComponentHash>::max();
    static EntityId      InvalidEntityId      = std::numeric_limits<EntityId>::max();

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

#define JaniNamespaceBegin(name)                  namespace name {
#define JaniNamespaceEnd(name)                    }

#define JANI_DISABLE_COPY(class_name) class_name(const class_name&) = delete;\
                                      class_name& operator=(const class_name&) = delete

#undef max
#undef min
