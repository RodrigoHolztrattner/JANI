////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include "nonstd/bitset_iter.h"

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

    using WorldCellCoordinates           = WorldPosition;
    using WorldCellCoordinatesHasher     = WorldPositionHasher;
    using WorldCellCoordinatesComparator = WorldPositionComparator;

    using WorkerId    = uint64_t;
    using ComponentId = uint64_t;
    using EntityId    = uint64_t;

    static const uint32_t MaximumLayers           = 32;
    static const uint32_t MaximumEntityComponents = 64;
    using                 ComponentMask           = std::bitset<MaximumEntityComponents>;

    static const uint32_t MaximumPacketSize = 4096;

    using Hash    = uint64_t;
    using LayerId = uint64_t;
}

// Set all members from the current struct/class as serializable
#define Serializable()                                                      \
template <class Archive>                                                    \
void serialize(Archive& ar)                                                 \
{                                                                           \
    boost::pfr::for_each_field(*this, [&ar](auto& field, std::size_t idx)   \
    {                                                                       \
        ar(field);                                                          \
    });                                                                     \
}

#define DISABLE_COPY(class_name) class_name(const class_name&) = delete; \
                                 class_name& operator=(const class_name&) = delete