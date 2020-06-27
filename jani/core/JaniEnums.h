////////////////////////////////////////////////////////////////////////////////
// Filename: JaniEnums.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>
#include "nonstd/enum_bitset.hpp"

namespace Jani
{
    enum class ComponentAttributeType
    {
        boolean,
        int32,
        int64,
        int32u,
        int64u,
        float32,
        float64,
        string
    };

    enum class WorkerLogLevel
    {
        Trace,
        Info,
        Warning,
        Error,
        Critical
    };

    enum class WorkerType
    {
        Dummy,
        Server, 
        Client
    };

    enum class LayerPermissionBits
    {
        None                    = 0,
        CanLog                  = 1 << 0,
        CanReserveEntityId      = 1 << 1,
        CanAddEntity            = 1 << 2,
        CanRemoveEntity         = 1 << 3,
        CanAddComponent         = 1 << 4,
        CanRemoveComponent      = 1 << 5,
        CanUpdateComponent      = 1 << 6,
        CanUpdateInterest       = 1 << 7, 
        CanReceiveQueryResults  = 1 << 8,
    };

    using LayerPermissionFlags = nonstd::enum_bitset<LayerPermissionBits>;

    enum class EntityFlagBits
    {
        None                 = 0,
        IsIncludedOnSnapshot = 1 << 0, 
        IsCellLocked         = 1 << 1, 
        IsPlayerControlable  = 1 << 2, 
    };

    using EntityFlags = nonstd::enum_bitset<EntityFlagBits>;

    enum class QueryUpdateFrequency
    {
        _50    = 50, 
        _40    = 40,
        _30    = 30,
        _20    = 20, 
        _10    = 10, 
        _5     = 5,
        _1     = 1,
        Low    = _5,
        Medium = _10, 
        High   = _40,
        Min    = _1, 
        Max    = _50, 
        Count  = 7
    };
}
