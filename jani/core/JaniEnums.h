////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>

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

    enum class LayerPermissions : uint64_t
    {
        Log = 1 << 0,
        ReserveEntityId = 1 << 1,
        AddEntity = 1 << 2,
        RemoveEntity = 1 << 3,
        AddComponent = 1 << 4,
        RemoveComponent = 1 << 5,
        UpdateComponent = 1 << 6,
        UpdateInterest = 1 << 7
    };

    enum class LayerLoadBalanceStrategyBits
    {
        None = 0,
        MaximumWorkers = 1 << 0,
        SpatialArea = 1 << 1
    };
}