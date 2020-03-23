////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <optional>
#include <unordered_set>
#include <variant>

#include "JaniTypes.h"
#include "JaniEnums.h"
#include "JaniLog.h"

#include "nonstd/enum_bitset.hpp"

#include <boost/pfr.hpp>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <entityx/entityx.h>
#include <cereal/types/vector.hpp>
#include <cereal/types/bitset.hpp>
#include <cereal/types/map.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/polymorphic.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/tuple.hpp>

namespace Jani
{
    struct WorldPosition
    {
        Serializable();

        bool operator==(const WorldPosition& rhs)
        {
            return (x == rhs.x && y == rhs.y);
        }

        bool operator() (const WorldPosition& rhs) const
        {
            if (x < rhs.x) return true;
            if (x > rhs.x) return false;
            //x == rhs.x
            if (y < rhs.y) return true;
            if (y > rhs.y) return false;

            return false;
        }

        friend bool operator <(const WorldPosition& lhs, const WorldPosition& rhs) //friend claim has to be here
        {
            if (lhs.x < rhs.x) return true;
            if (lhs.x > rhs.x) return false;
            //x == rhs.x
            if (lhs.y < rhs.y) return true;
            if (lhs.y > rhs.y) return false;

            return false;
        }

        operator glm::vec2()
        {
            return glm::vec2(x, y);
        }

        int32_t x, y;
    };

    struct WorldPositionHasher
    {
        size_t
            operator()(const WorldPosition& obj) const
        {
            return std::hash<int32_t>()(obj.x) ^ (std::hash<int32_t>()(obj.y) << 1) >> 1;
        }
    };

    struct WorldPositionComparator
    {
        bool operator()(const WorldPosition& obj1, const WorldPosition& obj2) const
        {
            if (obj1.x == obj2.x && obj1.y == obj2.y)
                return true;
            return false;
        }
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
            int32_t center_x = width - x;
            int32_t center_y = height - y;
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
        int32_t width = 0;
        int32_t height = 0;
    };

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
                update(const char* const data, const std::size_t size) noexcept
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
                update(const char* const data) noexcept
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

        static constexpr Hash FNV1A(const char* const _str, const std::size_t _size) noexcept
        {
            fnv1a_64 hashfn;;
            hashfn.update(_str, _size);
            return Hash(hashfn.digest());
        }

        static constexpr Hash FNV1A(const char* const _str) noexcept
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
        inline void data(const T* data, size_t size)
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

        inline void pointer(const void* ptr)
        {
            u64(reinterpret_cast<uintptr_t>(ptr));
        }

        template <typename ObjectType>
        inline void pointer(const ObjectType* _ptr)
        {
            return pointer(static_cast<const void*>(_ptr));
        }

        inline void string(const char* str)
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

    template <typename mType, uint32_t mBucketDimSize = 16>
    class SparseGrid
    {
        struct Bucket
        {
            std::array<std::array<std::optional<mType>, mBucketDimSize>, mBucketDimSize> cells;
        };
    public:
        SparseGrid(uint32_t _world_dim_size) : m_world_dim_size(_world_dim_size)
        {
            assert(m_world_dim_size % mBucketDimSize == 0);
            m_world_dim_size = _world_dim_size / mBucketDimSize;
            using size_type = typename std::allocator_traits< std::allocator<std::vector<std::unique_ptr<Bucket>>>>::size_type;
            size_type total_size = m_world_dim_size * m_world_dim_size;
            assert(total_size > 0 && total_size < std::numeric_limits<size_type>::max());
            m_buckets.resize(total_size);
        }
        bool IsCellEmpty(WorldCellCoordinates _cell_coordinates) const
        {
            auto [bucket_x, bucket_y, local_x, local_y] = ConvertWorldToLocalCoordinates(_cell_coordinates.x, _cell_coordinates.y);
            uint32_t index = (bucket_y / mBucketDimSize) * m_world_dim_size + (bucket_x / mBucketDimSize);
            return m_buckets[index] == nullptr || m_buckets[index]->cells[local_x][local_y] == std::nullopt;
        }
        bool Set(WorldCellCoordinates _cell_coordinates, mType _value)
        {
            if (!IsWorldPositionValid(_cell_coordinates.x, _cell_coordinates.y))
            {
                return false;
            }
            auto [bucket_x, bucket_y, local_x, local_y] = ConvertWorldToLocalCoordinates(_cell_coordinates.x, _cell_coordinates.y);
            auto& bucket = GetBucketAtBucketLocalPositionOrCreate(bucket_x, bucket_y);
            bucket.cells[local_x][local_y] = std::move(_value);
            m_total_active_cells++;
            return true;
        }
        void Clear(WorldCellCoordinates _cell_coordinates)
        {
            if (!IsWorldPositionValid(_cell_coordinates.x, _cell_coordinates.y))
            {
                return;
            }
            auto [bucket_x, bucket_y, local_x, local_y] = ConvertWorldToLocalCoordinates(_cell_coordinates.x, _cell_coordinates.y);
            auto& bucket = GetBucketAtBucketLocalPositionOrCreate(bucket_x, bucket_y);
            if (bucket != std::nullopt)
            {
                bucket = std::nullopt;
                m_total_active_cells--;
            }
        }
        const mType* TryAt(WorldCellCoordinates _cell_coordinates) const
        {
            return TryAtMutable(_cell_coordinates.x, _cell_coordinates.y);
        }
        const mType* TryAtMutable(WorldCellCoordinates _cell_coordinates) const
        {
            if (IsCellEmpty(_cell_coordinates.x, _cell_coordinates.y))
            {
                return nullptr;
            }
            return &AtMutable(_cell_coordinates);
        }
        const mType& At(WorldCellCoordinates _cell_coordinates) const
        {
            return AtMutable(_cell_coordinates);
        }
        mType& AtMutable(WorldCellCoordinates _cell_coordinates) const
        {
            auto [bucket_x, bucket_y, local_x, local_y] = ConvertWorldToLocalCoordinates(_cell_coordinates.x, _cell_coordinates.y);
            assert(IsBucketValid(bucket_x, bucket_y));
            return GetBucketAtBucketLocalPosition(bucket_x, bucket_y).cells[local_x][local_y].value();
        }
        const std::vector<mType>& InsideRect(WorldCellCoordinates _rect_coordinates_begin, WorldCellCoordinates _rect_coordinates_end) const
        {
            return InsideRectMutable(_rect_coordinates_begin, _rect_coordinates_end);
        }
        std::vector<mType>& InsideRectMutable(WorldCellCoordinates _rect_coordinates_begin, WorldCellCoordinates _rect_coordinates_end) const
        {
            WorldCellCoordinates central_coordinates = WorldCellCoordinates({
                (_rect_coordinates_begin.x + _rect_coordinates_end.x) / 2,
                (_rect_coordinates_begin.y + _rect_coordinates_end.y) / 2 });
            WorldCellCoordinates half_size = WorldCellCoordinates({
                static_cast<int32_t>(std::ceil((_rect_coordinates_end.x - _rect_coordinates_begin.x) / 2.0f)),
                static_cast<int32_t>(std::ceil((_rect_coordinates_end.y - _rect_coordinates_begin.y) / 2.0f)) });
            auto [bucket_x, bucket_y, local_x, local_y] = ConvertWorldToLocalCoordinates(central_coordinates.x, central_coordinates.y);
            int start_x = central_coordinates.x - std::max(1, half_size.x);
            int start_y = central_coordinates.y - std::max(1, half_size.y);
            int end_x = central_coordinates.x + std::max(1, half_size.x);
            int end_y = central_coordinates.y + std::max(1, half_size.y);
            m_query_temporary_buffer.clear();
            for (int x = start_x; x <= end_x; x++)
            {
                for (int y = start_y; y <= end_y; y++)
                {
                    if (IsCellEmpty(WorldCellCoordinates({ x, y })))
                    {
                        continue;
                    }
                    auto& bucket_value = AtMutable(WorldCellCoordinates({ x, y }));
                    m_query_temporary_buffer.push_back(bucket_value);
                }
            }
            return m_query_temporary_buffer;
        }

        const std::vector<mType>& InsideRange(WorldCellCoordinates _cell_coordinates, float _radius) const
        {
            return InsideRangeMutable(_cell_coordinates, _radius);
        }
        std::vector<mType>& InsideRangeMutable(WorldCellCoordinates _cell_coordinates, float _radius) const
        {
            auto [bucket_x, bucket_y, local_x, local_y] = ConvertWorldToLocalCoordinates(_cell_coordinates.x, _cell_coordinates.y);
            int actual_radius = std::ceil(_radius);
            int actual_radius_pow2 = actual_radius * actual_radius;
            int start_x = _cell_coordinates.x - actual_radius;
            int start_y = _cell_coordinates.y - actual_radius;
            int end_x = _cell_coordinates.x + actual_radius;
            int end_y = _cell_coordinates.y + actual_radius;
            m_query_temporary_buffer.clear();
            for (int x = start_x; x <= end_x; x++)
            {
                for (int y = start_y; y <= end_y; y++)
                {
                    if (IsCellEmpty(WorldCellCoordinates({ x, y })) ||
                        (x - _cell_coordinates.x + y - _cell_coordinates.y) * (x - _cell_coordinates.x + y - _cell_coordinates.y) > actual_radius_pow2)
                    {
                        continue;
                    }
                    auto& bucket_value = AtMutable(WorldCellCoordinates({ x, y }));
                    m_query_temporary_buffer.push_back(bucket_value);
                }
            }
            return m_query_temporary_buffer;
        }
        uint32_t GetTotalActiveCells() const
        {
            return m_total_active_cells;
        }
    private:
        Bucket& GetBucketAtBucketLocalPosition(int _x, int _y) const
        {
            uint32_t index = (_y / mBucketDimSize) * m_world_dim_size + (_x / mBucketDimSize);
            if (m_buckets[index] != nullptr)
            {
                return *m_buckets[index];
            }
            else
            {
                throw "Trying to retrieve bucket from invalid location, should create is false";
            }
        }
        Bucket& GetBucketAtBucketLocalPositionOrCreate(int _x, int _y)
        {
            uint32_t index = (_y / mBucketDimSize) * m_world_dim_size + (_x / mBucketDimSize);
            if (m_buckets[index] != nullptr)
            {
                return *m_buckets[index];
            }
            else
            {
                m_buckets[index] = std::make_unique<Bucket>();
                return *m_buckets[index];
            }
        }
        bool IsWorldPositionValid(int _x, int _y) const
        {
            return static_cast<uint32_t>(_x / mBucketDimSize) < m_world_dim_size&& static_cast<uint32_t>(_y / mBucketDimSize) < m_world_dim_size;
        }
        bool IsBucketValid(int _x, int _y) const
        {
            uint32_t index = (_y / mBucketDimSize) * m_world_dim_size + (_x / mBucketDimSize);
            return m_buckets[index] != nullptr;
        }
        std::tuple<int, int, int, int> ConvertWorldToLocalCoordinates(int _x, int _y) const
        {
            int bucket_x = _x / mBucketDimSize;
            int bucket_y = _y / mBucketDimSize;
            int local_x = _x % mBucketDimSize;
            int local_y = _y % mBucketDimSize;
            return { bucket_x, bucket_y, local_x, local_y };
        }
    private:
        uint32_t							 m_total_active_cells = 0;
        uint32_t							 m_world_dim_size = 0;
        std::vector<std::unique_ptr<Bucket>> m_buckets;
        mutable std::vector<mType>			 m_query_temporary_buffer;
    };

    struct WorkerRequestResult
    {
        explicit WorkerRequestResult(bool _succeed) : succeed(_succeed) {}
        WorkerRequestResult(bool _succeed, std::vector<int8_t> _payload) : succeed(_succeed), payload(std::move(_payload)) {}
        WorkerRequestResult(bool _succeed, int8_t* _payload_data, uint32_t _payload_size) : succeed(_succeed), payload(std::vector<int8_t>(_payload_data, _payload_data + _payload_size)) {}

        bool                succeed = false;
        std::vector<int8_t> payload;
    };

    struct ComponentQueryInstruction
    {
        Serializable();

        std::optional<ComponentMask> component_constraints;
        std::optional<WorldArea>     area_constraint;
        std::optional<WorldRect>     box_constraint;
        std::optional<uint32_t>      radius_constraint;

        // Only one is allowed
        std::optional<std::pair<std::unique_ptr<ComponentQueryInstruction>, std::unique_ptr<ComponentQueryInstruction>>> and_constraint;
        std::optional<std::pair<std::unique_ptr<ComponentQueryInstruction>, std::unique_ptr<ComponentQueryInstruction>>> or_constraint;

        void RequireComponent(ComponentId _component_id)
        {
            component_constraints = ComponentMask();
            component_constraints.value().set(_component_id, true);
        }

        void RequireComponents(ComponentMask _component_ids)
        {
            component_constraints = _component_ids;
        }

        void EntitiesInRect(WorldRect _rect)
        {
            box_constraint = _rect;
        }

        void EntitiesInArea(WorldArea _area)
        {
            area_constraint = _area;
        }

        void EntitiesInRadius(uint32_t _radius)
        {
            radius_constraint = _radius;
        }

        std::pair<ComponentQueryInstruction*, ComponentQueryInstruction*> And()
        {
            and_constraint = std::pair<std::unique_ptr<ComponentQueryInstruction>, std::unique_ptr<ComponentQueryInstruction>>();

            and_constraint.value().first = std::make_unique<ComponentQueryInstruction>();
            and_constraint.value().second = std::make_unique<ComponentQueryInstruction>();

            return { and_constraint.value().first.get(), and_constraint.value().second.get() };
        }

        std::pair<ComponentQueryInstruction*, ComponentQueryInstruction*> Or()
        {
            or_constraint = std::pair<std::unique_ptr<ComponentQueryInstruction>, std::unique_ptr<ComponentQueryInstruction>>();

            or_constraint.value().first = std::make_unique<ComponentQueryInstruction>();
            or_constraint.value().second = std::make_unique<ComponentQueryInstruction>();

            return { or_constraint.value().first.get(), or_constraint.value().second.get() };
        }
    };

    struct ComponentQuery
    {
        Serializable();

        friend Runtime;

        ComponentQueryInstruction* Begin(std::optional<ComponentId> _query_owner_component = std::nullopt)
        {
            query_owner_component = _query_owner_component;
            root_query = std::make_shared<ComponentQueryInstruction>();

            return root_query.get();
        }

        ComponentQuery& QueryComponent(ComponentId _component_id)
        {
            component_mask.set(_component_id);
            return *this;
        }

        ComponentQuery& QueryComponents(ComponentMask _component_mask)
        {
            component_mask = _component_mask;
            return *this;
        }

        ComponentQuery& WithFrequency(uint32_t _frequency)
        {
            frequency = std::max(frequency, _frequency);
            return *this;
        }

        bool IsValid() const
        {
            std::function<bool(const ComponentQueryInstruction&)> ValidateQueryInstruction = [&](const ComponentQueryInstruction& _query_instruction) -> bool
            {
                if (_query_instruction.box_constraint && _query_instruction.box_constraint->width >= 0 && _query_instruction.box_constraint->height >= 0)
                {
                    return true;
                }
                else if (_query_instruction.area_constraint && _query_instruction.area_constraint->width >= 0 && _query_instruction.area_constraint->height >= 0)
                {
                    return true;
                }
                else if (_query_instruction.radius_constraint)
                {
                    return _query_instruction.radius_constraint.value() >= 0.0f && _query_instruction.radius_constraint.value() < 10000000.0f;
                }
                else if (_query_instruction.component_constraints)
                {
                    return true;
                }
                else if (_query_instruction.and_constraint && _query_instruction.and_constraint->first && _query_instruction.and_constraint->second)
                {
                    if (!ValidateQueryInstruction(*_query_instruction.and_constraint->first))
                    {
                        return false;
                    }

                    if (!ValidateQueryInstruction(*_query_instruction.and_constraint->second))
                    {
                        return false;
                    }

                    return true;
                }
                else if (_query_instruction.or_constraint && _query_instruction.or_constraint->first && _query_instruction.or_constraint->second)
                {
                    return false; // Not supported for now
                    if (!ValidateQueryInstruction(*_query_instruction.or_constraint->first))
                    {
                        return false;
                    }

                    if (!ValidateQueryInstruction(*_query_instruction.or_constraint->second))
                    {
                        return false;
                    }

                    return true;
                }

                return false;
            };

            if (!root_query)
            {
                return false;
            }

            return ValidateQueryInstruction(*root_query);
        }

    public:

        std::optional<ComponentId>                 query_owner_component;
        ComponentMask                              component_mask;
        std::shared_ptr<ComponentQueryInstruction> root_query;
        uint32_t                                   frequency = 0;
    };

    struct ComponentQueryResultPayload
    {
        EntityId                      entity_id;
        ComponentMask                 entity_component_mask;
        WorldPosition                 entity_world_position;
        std::vector<ComponentPayload> component_payloads;
    };

    namespace Message
    {

        struct RuntimeClientAuthenticationRequest
        {
            Serializable();

            char     ip[16];
            uint32_t port;
            char     layer_name[128];
            uint64_t user_unique_id = 0;
            uint32_t access_token = 0;
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
            LayerId  layer_id = std::numeric_limits<LayerId>::max();
            uint32_t access_token = 0;
            uint32_t worker_authentication = 0;
        };

        struct RuntimeAuthenticationResponse
        {
            Serializable();

            bool     succeed = false;
            bool     use_spatial_area = false;
            uint32_t maximum_entity_limit = 0;
        };

        struct WorkerSpawnRequest
        {
            Serializable();

            std::string runtime_ip;
            uint32_t    runtime_worker_connection_port = 0;
            LayerId     layer_id = std::numeric_limits<LayerId>::max();
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

            bool succeed = false;
            EntityId id_begin = std::numeric_limits<EntityId>::max();
            EntityId id_end = std::numeric_limits<EntityId>::max();
        };

        // RuntimeAddEntity
        struct RuntimeAddEntityRequest
        {
            Serializable();

            EntityId                     entity_id;
            EntityPayload                entity_payload;
            std::optional<WorldPosition> entity_world_position;
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

        // RuntimeComponentInterestQueryUpdate
        struct RuntimeComponentInterestQueryUpdateRequest
        {
            Serializable();

            EntityId                    entity_id;
            ComponentId                 component_id;
            std::vector<ComponentQuery> queries;
        };

        // RuntimeComponentInterestQuery
        struct RuntimeComponentInterestQueryRequest
        {
            Serializable();

            EntityId    entity_id;
            ComponentId component_id;
        };

        // RuntimeComponentInterestQuery
        struct RuntimeComponentInterestQueryResponse
        {
            Serializable();

            bool                          succeed = false;
            ComponentMask                 entity_component_mask;
            std::vector<ComponentPayload> components_payloads;
        };

        // RuntimeInspectorQuery
        struct RuntimeInspectorQueryRequest
        {
            Serializable();

            uint64_t       window_id = std::numeric_limits<uint64_t>::max();
            WorldPosition  query_center_location;
            ComponentQuery query;
        };

        // RuntimeInspectorQuery
        struct RuntimeInspectorQueryResponse
        {
            Serializable();

            bool                          succeed = false;
            uint64_t                      window_id = std::numeric_limits<uint64_t>::max();
            EntityId                      entity_id = std::numeric_limits<EntityId>::max();
            ComponentMask                 entity_component_mask;
            WorldPosition                 entity_world_position;
            std::vector<ComponentPayload> components_payloads;
        };

        struct RuntimeComponentQueryResponse
        {
            Serializable();

            bool                          succeed = false;
            std::vector<ComponentPayload> component_payloads;
        };

        // RuntimeWorkerReportAcknowledge
        struct RuntimeWorkerReportAcknowledgeRequest
        {
            Serializable();

            uint64_t total_data_sent_per_second = 0;
            uint64_t total_data_received_per_second = 0;
        };

        struct RuntimeDefaultResponse
        {
            Serializable();

            bool succeed = false;
        };

        // WorkerLayerAuthorityGain
        struct WorkerLayerAuthorityGainRequest
        {
            Serializable();

            EntityId entity_id = std::numeric_limits<EntityId>::max();
        };

        // WorkerLayerAuthorityLost
        struct WorkerLayerAuthorityLostRequest
        {
            Serializable();

            EntityId entity_id = std::numeric_limits<EntityId>::max();
        };

        // WorkerAddComponent
        struct WorkerAddComponentRequest
        {
            Serializable();

            EntityId         entity_id = std::numeric_limits<EntityId>::max();
            ComponentId      component_id = std::numeric_limits<ComponentId>::max();
            ComponentPayload component_payload;
        };

        // WorkerRemoveComponent
        struct WorkerRemoveComponentRequest
        {
            Serializable();

            EntityId    entity_id = std::numeric_limits<EntityId>::max();
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
            using EntityInfo = std::tuple<EntityId, WorldPosition, WorkerId, std::bitset<MaximumEntityComponents>>;

            Serializable();

            bool                    succeed = false;
            std::vector<EntityInfo> entities_infos;
        };

        // RuntimeGetCellsInfos
        struct RuntimeGetCellsInfosRequest
        {
            Serializable();

            bool dummy = false;
        };

        // RuntimeGetWorkersInfo
        struct RuntimeGetCellsInfosResponse
        {
            using CellInfo = std::tuple<WorkerId, LayerId, WorldRect, WorldCellCoordinates, uint32_t>;

            Serializable();

            bool                  succeed = false;
            std::vector<CellInfo> cells_infos;
        };

        // RuntimeGetWorkersInfos
        struct RuntimeGetWorkersInfosRequest
        {
            Serializable();

            bool dummy = false;
        };

        // RuntimeGetWorkersInfos
        struct RuntimeGetWorkersInfosResponse
        {
            using WorkerInfo = std::tuple<WorkerId, uint32_t, LayerId, uint64_t, uint64_t, uint64_t, uint64_t>;

            Serializable();

            bool                    succeed = false;
            std::vector<WorkerInfo> workers_infos;
        };

    };

    struct WorkerCellsInfos
    {
        // The global entity count this worker has
        uint32_t entity_count = 0;

        // All the coordinates that this worker owns
        std::unordered_set<WorldCellCoordinates, WorldCellCoordinatesHasher, WorldCellCoordinatesComparator> coordinates_owned;

        RuntimeWorkerReference* worker_instance = nullptr;
    };

    struct WorldCellInfo
    {
        std::map<EntityId, ServerEntity*>                  entities;
        std::array<WorkerCellsInfos*, MaximumLayers> worker_cells_infos;
        WorldCellCoordinates                         cell_coordinates;

        std::optional<RuntimeWorkerReference*> GetWorkerForLayer(LayerId _layer_id) const
        {
            assert(_layer_id < MaximumLayers);

            if (!worker_cells_infos[_layer_id] || !worker_cells_infos[_layer_id]->worker_instance)
            {
                return std::nullopt;
            }

            return worker_cells_infos[_layer_id]->worker_instance;
        }
    };

    class ServerEntity
    {
        DISABLE_COPY(ServerEntity);

    public:

        ServerEntity(EntityId _unique_id) : entity_id(_unique_id)
        {

        }

        /*
        * Update this entity world cell coordinates
        * This should only be called by the world controller
        */
        void SetWorldCellInfo(const WorldCellInfo& _world_cell) // TODO: Make this protected
        {
            world_cell_info = &_world_cell;
        }

        /*
        * Return a reference to the current world cell info
        */
        const WorldCellInfo& GetWorldCellInfo() const
        {
            assert(world_cell_info);
            return *world_cell_info;
        }

        /*
        * Return the world cell coordinate that this entity is inside
        * This coordinate will only be valid if this entity had at least one
        * positional update, it has no meaning and will point to 0, 0 if no
        * spatial info is present
        */
        WorldCellCoordinates GetWorldCellCoordinates() const
        {
            return world_cell_info->cell_coordinates;
        }

        /*

        */
        WorldPosition GetWorldPosition() const
        {
            return world_position;
        }

        /*

        */

        /*

        */
        /*
            WorkerId GetRepresentativeWorldPositionWorker() const
            {
                return world_position_worker_owner;
            }
        */

        /*

        */
        // This should not be used, I think
        void SetWorldPosition(WorldPosition _position, WorkerId _position_worker)
        {
            world_position = _position;
            world_position_worker_owner = _position_worker;
        }

        /*

        */
        EntityId GetId() const
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

        const ComponentMask& GetComponentMask() const
        {
            return component_mask;
        }

        void AddComponent(ComponentId _component_id, ComponentPayload _payload)
        {
            component_mask[_component_id] = true;
            m_component_payloads[_component_id] = std::move(_payload);
        }

        bool UpdateComponent(ComponentId _component_id, ComponentPayload _payload)
        {
            if (!component_mask[_component_id])
            {
                return false;
            }

            m_component_payloads[_component_id] = std::move(_payload);
            return true;
        }

        std::optional<ComponentPayload> GetComponentPayload(ComponentId _component_id) const
        {
            if (!component_mask[_component_id])
            {
                return std::nullopt;
            }

            return m_component_payloads[_component_id];
        }

        void RemoveComponent(ComponentId _component_id)
        {
            component_mask[_component_id] = false;
            m_component_payloads[_component_id] = ComponentPayload();

            assert(_component_id < MaximumEntityComponents);

            // TODO: Somehow check if the entity still have enough components to justify a layer being owned by a worker
        }

        /*
        * Update the queries for a given component slot
        */
        void UpdateQueriesForComponent(ComponentId _component_id, std::vector<ComponentQuery> _queries) // Could use a transient vector, clear the query one and copy into the already existent memory
        {
            assert(_component_id < MaximumEntityComponents);
            m_component_queries[_component_id] = std::move(_queries);
            m_component_queries_version[_component_id]++;
        }

        /*
        * Return a reference to the component query vector
        */
        const std::vector<ComponentQuery>& GetQueriesForComponent(ComponentId _component_id) const
        {
            assert(_component_id < MaximumEntityComponents);
            return m_component_queries[_component_id];
        }

        /*
        * Return the version for the given component query
        * This is usually used to identify out of date queries
        */
        uint32_t GetQueryVersion(ComponentId _component_id)
        {
            return m_component_queries_version[_component_id];
        }

        ComponentId    component_id;
        ComponentQuery query;

    private:

        EntityId      entity_id = std::numeric_limits<EntityId>::max();
        WorldPosition world_position = { 0, 0 };
        WorkerId      world_position_worker_owner = std::numeric_limits<WorkerId>::max();
        ComponentMask component_mask;
        const WorldCellInfo* world_cell_info = nullptr;


        std::array<ComponentPayload, MaximumEntityComponents>            m_component_payloads;
        std::array<std::vector<ComponentQuery>, MaximumEntityComponents> m_component_queries;
        std::array<uint32_t, MaximumEntityComponents>                    m_component_queries_version = {};
    };

    template <typename C, typename EM>
    using ComponentHandle = entityx::ComponentHandle<C, EM>;
}

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