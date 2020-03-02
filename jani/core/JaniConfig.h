////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdint>
#include <string>
#include <optional>
#include <filesystem>
#include <functional>
#include <iostream>
#include <fstream>
#include <set>
#include <thread>
#include <limits>
#include <mutex>
#include <string>
#include <variant>
#include <bitset>
#include <entityx/entityx.h>
#include <boost/pfr.hpp>
#include <magic_enum.hpp>
#include <ctti/type_id.hpp>
#include <ctti/static_value.hpp>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <nlohmann/json.hpp>
#include "span.hpp"

#include <ikcp.h>

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/bitset.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/utility.hpp>

#include "JaniConnection.h"

/*
    TODO LIST:

    - QBI
    - Setup layer permissions on the config file
    - Check for worker/layer permissions before doing any action
    - Setup component authority when an entity or component is added
    - Check when a component/layer should have its authority worker changed
    - Setup messages for QBI updates
    - Process the QBI (query) messages (instead of sending the query, pass only the id/component id/layer id)
    - Implement authority messages
    - Implement authority check/change
    - Implement authority handling on the worker side
    - Implement worker disconnection recovery
*/

/////////////
// DEFINES //
/////////////

#define JaniNamespaceBegin(name)                  namespace name {
#define JaniNamespaceEnd(name)                    }

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

#define DISABLE_COPY(class_name) class_name(const class_name&) = delete;\
                                 class_name& operator=(const class_name&) = delete

JaniNamespaceBegin(Jani)

class Bridge;
class Runtime;
class Database;
class WorkerSpawnerInstance;

struct WorldPosition
{
    Serializable();

    int32_t x, y;
};

struct WorldArea
{
    Serializable();

    uint32_t width, height;
};

struct WorldRect
{
    Serializable();

    uint32_t ManhattanDistanceFromPosition(WorldPosition _position) const
    {
        int32_t center_x  = width - x;
        int32_t center_y  = height - y;
        uint32_t distance = std::abs(center_x - _position.x) + std::abs(center_y - _position.y);
        uint32_t max_side = std::max(width, height);

        return static_cast<int32_t>(distance) - static_cast<int32_t>(max_side / 2) < 0 ? 0 : distance - max_side / 2;
    }

    int32_t AreaDifference(const WorldRect& _other_rect) const
    {
        return width * height - _other_rect.width * _other_rect.height;
    }

    int32_t x = 0;
    int32_t y = 0;
    int32_t width  = 0;
    int32_t height = 0;
};

static void to_json(nlohmann::json& j, const Jani::WorldPosition& _object)
{
    j = nlohmann::json
    {
        {"x", _object.x},
        {"y", _object.y}
    };
}

static void from_json(const nlohmann::json& j, Jani::WorldPosition& _object)
{
    j.at("x").get_to(_object.x);
    j.at("y").get_to(_object.y);
}

static void to_json(nlohmann::json& j, const Jani::WorldArea& _object)
{
    j = nlohmann::json
    {
        {"width", _object.width},
        {"height", _object.height}
    };
}

static void from_json(const nlohmann::json& j, Jani::WorldArea& _object)
{
    j.at("width").get_to(_object.width);
    j.at("height").get_to(_object.height);
}

static void to_json(nlohmann::json& j, const Jani::WorldRect& _object)
{
    j = nlohmann::json
    {
        {"x", _object.x},
        {"y", _object.y},
        {"width", _object.width},
        {"height", _object.height}
    };
}

static void from_json(const nlohmann::json& j, Jani::WorldRect& _object)
{
    j.at("x").get_to(_object.x);
    j.at("y").get_to(_object.y);
    j.at("width").get_to(_object.width);
    j.at("height").get_to(_object.height);
}

class User
{
public:

    /*
    
    */
    WorldPosition GetRepresentativeWorldPosition() const;

private:

    WorldPosition user_world_position;
};

using WorkerId = uint64_t;

using ComponentId = uint64_t;

using EntityId = uint64_t;

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

struct WorkerRequestResult
{
    explicit WorkerRequestResult(bool _succeed) : succeed(_succeed) {}
    WorkerRequestResult(bool _succeed, std::vector<int8_t> _payload) : succeed(_succeed), payload(std::move(_payload)) {}
    WorkerRequestResult(bool _succeed, int8_t* _payload_data, uint32_t _payload_size) : succeed(_succeed), payload(std::vector<int8_t>(_payload_data, _payload_data + _payload_size)) {}

    bool                succeed = false;
    std::vector<int8_t> payload;
};

/*
* 1. Entity who owns the component
* 2. The component id
* 3. The worker who has the interest in receiving component updates
*/
using ComponentUpdateReadInterest = std::tuple<EntityId, ComponentId, WorkerId>;

struct ComponentPayload
{
    Serializable();

    template <typename PayloadType>
    void SetPayload(const PayloadType& _payload)
    {
        component_data.resize(sizeof(PayloadType));
        std::memcpy(component_data.data(), &_payload, sizeof(PayloadType));
    }

    template <typename PayloadType>
    const PayloadType& GetPayload() const
    {
        assert(component_data.size() == sizeof(PayloadType));
        return *reinterpret_cast<const PayloadType*>(component_data.data());
    }

    EntityId            entity_owner;
    ComponentId         component_id;
    std::vector<int8_t> component_data;
};

struct EntityPayload
{
    Serializable();

    std::vector<ComponentPayload> component_payloads;
};

static const uint32_t MaximumLayers           = 32;

static const uint32_t MaximumEntityComponents = 64;
using ComponentMask = std::bitset<MaximumEntityComponents>;

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

enum class LayerLoadBalanceStrategyBits
{
    None           = 0,
    MaximumWorkers = 1 << 0, 
    SpatialArea    = 1 << 1
};

using LayerLoadBalanceStrategyTypeFlags = enum_bitset<LayerLoadBalanceStrategyBits>;

struct LayerLoadBalanceStrategy
{
    std::optional<uint32_t>  maximum_workers;
    std::optional<WorldArea> minimum_area;
    std::optional<WorldArea> maximum_area;
};

static const uint32_t MaximumPacketSize = 4096;

class RuntimeInterface
{

};

/*
* Communication between a worker and a bridge
*/
class BridgeInterface
{
public:

    BridgeInterface(int _local_port, int _bridge_port, const char* _bridge_address) :
        m_bridge_connection(_local_port, _bridge_port, _bridge_address),
        m_temporary_data_buffer(std::array<char, MaximumPacketSize>())
    {

    }

    void PoolIncommingData(const Connection<>::ReceiveCallback& _receive_callback)
    {
        m_bridge_connection.Receive(_receive_callback);
    }

    bool PushData(const char* _data, size_t _data_size)
    {
        return m_bridge_connection.Send(_data, _data_size);
    }

private:

    Connection<>                        m_bridge_connection;
    std::array<char, MaximumPacketSize> m_temporary_data_buffer;
};

using Hash      = uint64_t;
using LayerId   = uint64_t;

// A hasher object
struct Hasher
{
private:

	template <typename ResultT, ResultT OffsetBasis, ResultT Prime>
	class basic_fnv1a final
	{
		static_assert(std::is_unsigned<ResultT>::value, "need unsigned integer");
		using result_type = ResultT;
		result_type state_{};

	public:

		constexpr
			basic_fnv1a() noexcept : state_{ OffsetBasis }
		{
		}

		constexpr void
			update(const char *const data, const std::size_t size) noexcept
		{
			auto acc = this->state_;
			for (auto i = std::size_t{}; i < size; ++i)
			{
				const auto next = std::size_t(data[i]);
				acc = ResultT((acc ^ next) * Prime);
			}
			this->state_ = acc;
		}

		constexpr void
			update(const char *const data) noexcept
		{
			auto acc = this->state_;
			for (auto i = std::size_t{}; data[i] != 0; ++i)
			{
				const auto next = std::size_t(data[i]);
				acc = ResultT((acc ^ next) * Prime);
			}
			this->state_ = acc;
		}

		constexpr result_type
			digest() const noexcept
		{
			return this->state_;
		}
	};

	using fnv1a_32 = basic_fnv1a<Hash,
		UINT32_C(2166136261),
		UINT32_C(16777619)>;

	using fnv1a_64 = basic_fnv1a<Hash,
		UINT64_C(14695981039346656037),
		UINT64_C(1099511628211)>;

public:

    Hasher()
    {}
    Hasher(Hash _hash) : h(_hash)
    {}
    Hasher(uint32_t _value)
    {
        u32(_value);
    }
    Hasher(float _value)
    {
        f32(_value);
    }
    Hasher(const void* _ptr)
    {
        pointer(_ptr);
    }
    Hasher(const char* _str)
    {
        string(_str);
    }
    Hasher(const std::string& _str)
    {
        string(_str.c_str());
    }

    inline Hasher& operator()(bool _value)
    {
        u32(static_cast<uint32_t>(_value));
        return *this;
    }

    inline Hasher& operator()(uint32_t _value)
    {
        u32(_value);
        return *this;
    }

    inline Hasher& operator()(int32_t _value)
    {
        u32(static_cast<uint32_t>(_value));
        return *this;
    }

    inline Hasher& operator()(uint64_t _value)
    {
        u64(_value);
        return *this;
    }

    inline Hasher& operator()(int64_t _value)
    {
        u64(static_cast<uint64_t>(_value));
        return *this;
    }

    inline Hasher& operator()(float _value)
    {
        f32(_value);
        return *this;
    }

    inline Hasher& operator()(double _value)
    {
        f64(_value);
        return *this;
    }

    template <typename T>
    inline Hasher& operator()(const T* _data, size_t _size)
    {
        data(_data, _size);
        return *this;
    }

    template <typename T>
    inline Hasher& operator()(T* _data, size_t _size)
    {
        data(_data, _size);
        return *this;
    }

    inline Hasher& operator()(const void* _ptr)
    {
        u64(reinterpret_cast<uintptr_t>(_ptr));
        return *this;
    }

    template <typename ObjectType>
    inline Hasher& operator()(const ObjectType* _ptr)
    {
        pointer(static_cast<const void*>(_ptr));
        return *this;
    }

    inline Hasher& operator()(const char* _str)
    {
#ifndef NDEBUG
        int overflow_counter = 0;
#endif
        char c;
        while ((c = *_str++) != '\0')
        {
            u32(uint8_t(c));
#ifndef NDEBUG
            overflow_counter++;
            if (overflow_counter > 1000000)
            {
                assert(false);
            }
#endif
        }
        return *this;
    }

    inline Hasher& operator()(const std::string& _str)
    {
        string(_str.c_str());
        return *this;
    }

    // Container operators

    template <class _T>
    inline Hasher& operator()(const std::vector<_T>& _vector)
    {
        for (auto& value : _vector)
        {
            this->operator()(value);
        }
        return *this;
    }

    template <class _Key, class _T>
    inline Hasher& operator()(const std::map<_Key, _T>& _map)
    {
        for (auto& [key, value] : _map)
        {
            this->operator()(key);
            this->operator()(value);
        }
        return *this;
    }

    template <class _Key, class _T>
    inline Hasher& operator()(const std::unordered_map<_Key, _T>& _map)
    {
        for (auto& [key, value] : _map)
        {
            this->operator()(key);
            this->operator()(value);
        }
        return *this;
    }

    template <class ... Types>
    inline Hasher& operator()(const std::variant<Types ...>& _variant)
    {
        std::visit([&](auto&& _arg) 
        {
            this->operator()(_arg); 
        }, _variant);
        return *this;
    }

    template <class _T1, class _T2>
    inline Hasher& operator()(const std::pair<_T1, _T2>& _a)
    {
        this->operator()(_a.first);
        this->operator()(_a.second);
        return *this;
    }

    /*
    // Only selects this if n parameters > 1
    template<typename... Types>
    Hasher& operator()(Types&& ... args)
    {
        for_each_arg(args...);
        return *this;
    }
    */

    template <typename T, class = typename std::enable_if< std::is_enum<T>::value >::type>
    inline Hasher& operator()(const T& _value)
    {
        this->operator()(static_cast<int>(_value));
        return *this;
    }

    inline Hash get() const
    {
        return h;
    }

    inline operator Hash() const
    {
        return h;
    }

    struct StructType
    {
        int a;
    };

    template <class _T>
    inline Hasher& struct_visit(const _T& _struct)
    {
        boost::pfr::for_each_field(_struct, [&](auto& _field, std::size_t _idx)
        {
            this->operator()(_field);
        });

        return *this;
    }

    template <class _T>
    inline Hasher& struct_visit_flat(const _T& _struct)
    {
        boost::pfr::flat_for_each_field(_struct, [&](auto& _field, std::size_t _idx)
        {
            this->operator()(_field);
        });

        return *this;
    }

    static Hash default_value()
    {
        return 0xcbf29ce484222325ull;
    }

private:

    template<class...Args>
    void for_each_arg(Args&& ...args) 
    {
        (this->operator()(std::forward<Args>(args)), ...);
    }
                
	static constexpr Hash FNV1A(const char *const _str, const std::size_t _size) noexcept
	{
		fnv1a_64 hashfn;;
		hashfn.update(_str, _size);
		return Hash(hashfn.digest());
	}

	static constexpr Hash FNV1A(const char *const _str) noexcept
	{
		fnv1a_64 hashfn;;
		hashfn.update(_str);
		return Hash(hashfn.digest());
	}

    static constexpr Hash FNV1A(const std::string& _str) noexcept
    {
        fnv1a_64 hashfn;;
        hashfn.update(_str.c_str());
        return Hash(hashfn.digest());
    }
		
	template <typename T>
	inline void data(const T *data, size_t size)
	{
		size /= sizeof(*data);
		for (size_t i = 0; i < size; i++)
			h = (h * 0x100000001b3ull) ^ data[i];
	}

	inline void u32(uint32_t value)
	{
		h = (h * 0x100000001b3ull) ^ value;
	}

	inline void f32(float value)
	{
		union
		{
			float f32;
			uint32_t u32;
		} u;
		u.f32 = value;
		u32(u.u32);
	}

    inline void f64(float value)
    {
        union
        {
            float f64;
            uint32_t u64;
        } u;
        u.f64 = value;
        u64(u.u64);
    }

	inline void u64(uint64_t value)
	{
		u32(value & 0xffffffffu);
		u32(value >> 32);
	}

	inline void hash(Hash value)
	{
		u64(value);
	}

	inline void pointer(const void *ptr)
	{
		u64(reinterpret_cast<uintptr_t>(ptr));
	}

	template <typename ObjectType>
	inline void pointer(const ObjectType* _ptr)
	{
		return pointer(static_cast<const void*>(_ptr));
	}

	inline void string(const char *str)
	{
		char c;
		while ((c = *str++) != '\0')
			u32(uint8_t(c));
	}

    inline void string(const std::string& str)
    {
        string(str.c_str());
    }

private:

	Hash h = default_value();
};

struct ComponentQueryInstruction
{   
    Serializable();

    std::vector<ComponentId> component_constraints;
    ComponentMask            component_constraints_mask = 0;

    std::optional<WorldArea> area_constraint;
    std::optional<uint32_t>  radius_constraint;

    // Only one is allowed
    bool and_constraint = false;
    bool or_constraint  = false;

    // If linked with multiple instructions, the frequency from the last one will be
    // used instead
    uint32_t frequency = 1;
};

struct ComponentQuery
{
    Serializable();

    friend Runtime;

    ComponentQuery& Begin(ComponentId _query_owner_component, std::optional<WorldPosition> _query_position)
    {
        query_owner_component =_query_owner_component;
        query_position        = _query_position;

        return *this;
    }

    ComponentQuery& QueryComponent(ComponentId _component_id)
    {
        query_component_ids.push_back(_component_id);
        return *this;
    }

    ComponentQuery& QueryComponents(std::vector<ComponentId> _component_ids)
    {
        query_component_ids.insert(query_component_ids.end(), _component_ids.begin(), _component_ids.end());
        return *this;
    }

    ComponentQuery& WhereEntityOwnerHasComponent(ComponentId _component_id)
    { 
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        current_query.component_constraints.push_back(_component_id);
        return *this;
    }

    ComponentQuery& WhereEntityOwnerHasComponents(std::vector<ComponentId> _component_ids)
    {
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        current_query.component_constraints.insert(current_query.component_constraints.end(), _component_ids.begin(), _component_ids.end());
        return *this;
    }

    ComponentQuery& InArea(WorldArea _area)
    {
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        assert(!current_query.radius_constraint);
        current_query.area_constraint = _area;
        return *this;
    }

    ComponentQuery& InRadius(uint32_t _radius)
    {
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        assert(!current_query.area_constraint);
        current_query.radius_constraint = _radius;
        return *this;
    }

    ComponentQuery& WithFrequency(uint32_t _frequency)
    {
        auto& current_query = AddOrGetQuery();

        assert(query_position);
        current_query.frequency = _frequency;
        return *this;
    }

    ComponentQuery& And()
    {
        auto& current_query = AddOrGetQuery();

        assert(!current_query.or_constraint);

        current_query.and_constraint = true;
        AddOrGetQuery(true);
        return *this;
    }

    ComponentQuery& Or()
    {
        auto& current_query = AddOrGetQuery();

        assert(!current_query.and_constraint);

        current_query.or_constraint = true;
        AddOrGetQuery(true);
        return *this;
    }

protected:

    ComponentQueryInstruction& AddOrGetQuery(bool _force_add = false)
    {
        if (queries.size() == 0 || _force_add)
        {
            queries.push_back(ComponentQueryInstruction());
        }
        else
        {
            return queries.back();
        }
    }

public:

    ComponentId                            query_owner_component;
    std::optional<WorldPosition>           query_position;
    std::vector<ComponentId>               query_component_ids;
    ComponentMask                          component_mask = 0;
    std::vector<ComponentQueryInstruction> queries;
};

JaniNamespaceBegin(Message)

struct RuntimeClientAuthenticationRequest
{
    Serializable();

    char     ip[16];
    uint32_t port;
    char     layer_name[128];
    uint64_t user_unique_id       = 0;
    uint32_t access_token         = 0;
    uint32_t authentication_token = 0;
};

struct RuntimeClientAuthenticationResponse
{
    Serializable();

    bool succeed = false;
};

struct RuntimeAuthenticationRequest
{
    Serializable();

    char     ip[16];
    uint32_t port;
    LayerId  layer_id              = std::numeric_limits<LayerId>::max();
    uint32_t access_token          = 0;
    uint32_t worker_authentication = 0;
};

struct RuntimeAuthenticationResponse
{
    Serializable();

    bool     succeed              = false;
    bool     use_spatial_area     = false;
    uint32_t maximum_entity_limit = 0;
};

struct WorkerSpawnRequest
{
    Serializable();

    std::string runtime_ip;
    uint32_t    runtime_worker_connection_port = 0;
    LayerId     layer_id                       = std::numeric_limits<LayerId>::max();
};

struct WorkerSpawnResponse
{
    Serializable();

    bool succeed = false;
};

// RuntimeLogMessage
struct RuntimeLogMessageRequest
{
    Serializable();

    WorkerLogLevel log_level;
    std::string    log_title;
    std::string    log_message;
};

// RuntimeReserveEntityIdRange
struct RuntimeReserveEntityIdRangeRequest
{
    Serializable();

    uint32_t total_ids;
};

// RuntimeReserveEntityIdRange
struct RuntimeReserveEntityIdRangeResponse
{
    Serializable();

    bool succeed      = false;
    EntityId id_begin = std::numeric_limits<EntityId>::max();
    EntityId id_end   = std::numeric_limits<EntityId>::max();
};

// RuntimeAddEntity
struct RuntimeAddEntityRequest
{
    Serializable();

    EntityId      entity_id;
    EntityPayload entity_payload;
};

// RuntimeRemoveEntity
struct RuntimeRemoveEntityRequest
{
    Serializable();

    EntityId entity_id;
};

// RuntimeAddComponent
struct RuntimeAddComponentRequest
{
    Serializable();

    EntityId         entity_id;
    ComponentId      component_id;
    ComponentPayload component_payload;
};

// RuntimeRemoveComponent
struct RuntimeRemoveComponentRequest
{
    Serializable();

    EntityId    entity_id;
    ComponentId component_id;
};

// RuntimeComponentUpdate
struct RuntimeComponentUpdateRequest
{
    Serializable();

    EntityId                     entity_id;
    ComponentId                  component_id;
    ComponentPayload             component_payload;
    std::optional<WorldPosition> entity_world_position;
};

// RuntimeComponentQuery
struct RuntimeComponentQueryRequest
{
    Serializable();

    ComponentQuery query;
};

struct RuntimeComponentQueryResponse
{
    Serializable();

    bool                            succeed = false;
    std::optional<ComponentPayload> component_payload;
};

// RuntimeWorkerReportAcknowledge
struct RuntimeWorkerReportAcknowledgeRequest
{
    Serializable();

    std::optional<EntityId>  extreme_top_entity;
    std::optional<EntityId>  extreme_right_entity;
    std::optional<EntityId>  extreme_left_entity;
    std::optional<EntityId>  extreme_bottom_entity;
    uint32_t                 total_entities_over_capacity = 0;
    std::optional<WorldRect> worker_rect;
};

struct RuntimeDefaultResponse
{
    Serializable();

    bool succeed = false;
};

// WorkerAddComponent
struct WorkerAddComponentRequest
{
    Serializable();

    EntityId         entity_id    = std::numeric_limits<EntityId>::max();
    ComponentId      component_id = std::numeric_limits<ComponentId>::max();
    ComponentPayload component_payload;
};

// WorkerRemoveComponent
struct WorkerRemoveComponentRequest
{
    Serializable();

    EntityId    entity_id    = std::numeric_limits<EntityId>::max();
    ComponentId component_id = std::numeric_limits<ComponentId>::max();
};

struct WorkerDefaultResponse
{
    Serializable();

    bool succeed = false;
};

// RuntimeGetEntitiesInfo
struct RuntimeGetEntitiesInfoRequest
{
    Serializable();

    bool dummy = false;
};

// RuntimeGetEntitiesInfo
struct RuntimeGetEntitiesInfoResponse
{
    Serializable();

    bool                                                       succeed = false;
    std::vector<std::tuple<EntityId, WorldPosition, WorkerId>> entities_infos;
};

// RuntimeGetWorkersInfo
struct RuntimeGetWorkersInfoRequest
{
    Serializable();

    bool dummy = false;
};

// RuntimeGetWorkersInfo
struct RuntimeGetWorkersInfoResponse
{
    Serializable();

    bool                                                                           succeed = false;
    std::vector<std::tuple<WorkerId, LayerId, WorldRect, uint32_t>> worker_infos;
};

JaniNamespaceEnd(Message)

class Entity
{
    DISABLE_COPY(Entity);

public:

    Entity(EntityId _unique_id) : entity_id(_unique_id) 
    {

    }

    /*

    */
    WorldPosition GetRepresentativeWorldPosition() const
    {
        return world_position;
    }

    /*
    
    */
    WorkerId GetRepresentativeWorldPositionWorker() const
    {
        return world_position_worker_owner;
    }

    /*
    
    */
    void SetRepresentativeWorldPosition(WorldPosition _position, WorkerId _position_worker)
    {
        world_position              = _position;
        world_position_worker_owner = _position_worker;
    }

    /*
    
    */
    EntityId GetUniqueId() const
    {
        return entity_id;
    }

    /*
    
    */
    bool HasComponent(ComponentId _component_id) const
    {
        return component_mask.test(_component_id);
    }
    bool HasComponents(ComponentMask _component_mask) const
    {
        for (int i = 0; i < MaximumEntityComponents; i++)
        {
            if (component_mask[i] && !_component_mask[i])
            {
                return false;
            }
        }

        return true;
    }

    void AddComponent(ComponentId _component_id)
    {
        component_mask[_component_id] = true;
    }

    void RemoveComponent(ComponentId _component_id)
    {
        component_mask[_component_id] = false;

        // Function that maps from component to layer
        uint32_t layer_id = -1;

        assert(_component_id < MaximumEntityComponents);
        assert(layer_id < MaximumLayers);

        m_worker_authority_info[layer_id][_component_id] = std::nullopt;
    }

    void AddQueriesForLayer(uint32_t _layer_id, std::vector<ComponentQuery> _queries)
    {
        return UpdateQueriesForLayer(_layer_id, std::move(_queries)); // Add and update is basically the same since we will always receive the entire layer query data
    }

    void RemoveQueriesForLayer(uint32_t _layer_id)
    {
        assert(_layer_id < MaximumLayers);
        m_layer_queries[_layer_id].clear();
    }

    void UpdateQueriesForLayer(uint32_t _layer_id, std::vector<ComponentQuery> _queries) // Could use a transient vector, clear the query one and copy into the already existent memory
    {
        assert(_layer_id < MaximumLayers);
        m_layer_queries[_layer_id] = std::move(_queries);
    }

    void SetComponentAuthority(LayerId _layer_id, ComponentId _component_id, WorkerId _worker_id)
    {
        assert(_component_id < MaximumEntityComponents);
        assert(_layer_id < MaximumLayers);

        m_worker_authority_info[_layer_id][_component_id] = _worker_id;
    }

    std::optional<WorkerId> GetComponentAuthority(LayerId _layer_id, ComponentId _component_id) const
    {
        assert(_component_id < MaximumEntityComponents);
        assert(_layer_id < MaximumLayers);

        return m_worker_authority_info[_layer_id][_component_id];
    }

    bool VerifyWorkerComponentAuthority(LayerId _layer_id, ComponentId _component_id, WorkerId _worker_id) const
    {
        assert(_component_id < MaximumEntityComponents);
        assert(_layer_id < MaximumLayers);

        return m_worker_authority_info[_layer_id][_component_id] ? m_worker_authority_info[_layer_id][_component_id].value() == _worker_id : false;
    }

private:

    EntityId      entity_id                   = std::numeric_limits<EntityId>::max();
    WorldPosition world_position              = { 0, 0 };
    WorkerId      world_position_worker_owner = std::numeric_limits<WorkerId>::max();
    ComponentMask component_mask;

    std::array<std::array<std::optional<WorkerId>, MaximumEntityComponents>, MaximumLayers> m_worker_authority_info;

    std::array<std::vector<ComponentQuery>, MaximumLayers> m_layer_queries;
};

JaniNamespaceEnd(Jani)

#undef max
#undef min