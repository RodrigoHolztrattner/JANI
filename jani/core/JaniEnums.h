////////////////////////////////////////////////////////////////////////////////
// Filename: JaniEnums.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <cstdint>

namespace Jani
{

template <typename EnumType, typename EnumBitsType = uint32_t>
class enum_bitset
{
public:

    enum_bitset()  = default;
    ~enum_bitset() = default;

    explicit enum_bitset(EnumType _enum_value) : m_bitset(static_cast<EnumBitsType>(_enum_value))
    {
    }

    bool operator ==(EnumBitsType _bits) const
    {
        return m_bitset == static_cast<EnumBitsType>(_bits);
    }

    template <typename OtherEnumType, typename OtherEnumBitsType>
    bool operator ==(const enum_bitset<OtherEnumType, OtherEnumBitsType>& _bits) const
    {
        return m_bitset == _bits.get_raw();
    }

    bool operator !=(EnumBitsType _bits) const
    {
        return !(*this == _bits);
    }

    template <typename OtherEnumType, typename OtherEnumBitsType>
    bool operator !=(const enum_bitset<OtherEnumType, OtherEnumBitsType>& _bits) const
    {
        return !(*this == _bits);
    }

    enum_bitset operator <<(EnumType _enum)
    {
        m_bitset = m_bitset | static_cast<EnumBitsType>(_enum);
        return *this;
    }

    enum_bitset operator <<(EnumBitsType _bits)
    {
        m_bitset = m_bitset | _bits;
        return *this;
    }

    bool operator &(EnumType _bits) const
    {
        return m_bitset & static_cast<EnumBitsType>(_bits);
    }

    operator EnumType() const
    {
        return static_cast<EnumType>(m_bitset);
    }

    EnumBitsType get_raw() const
    {
        return m_bitset;
    }

private:

    EnumBitsType m_bitset = 0;
};


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
    Log             = 1 << 0, 
    ReserveEntityId = 1 << 1,
    AddEntity       = 1 << 2, 
    RemoveEntity    = 1 << 3,
    AddComponent    = 1 << 4,
    RemoveComponent = 1 << 5,
    UpdateComponent = 1 << 6,
    UpdateInterest  = 1 << 7
};

enum class LayerLoadBalanceStrategyBits
{
    None           = 0,
    MaximumWorkers = 1 << 0, 
    SpatialArea    = 1 << 1
};

using LayerLoadBalanceStrategyTypeFlags = enum_bitset<LayerLoadBalanceStrategyBits>;

} // Jani