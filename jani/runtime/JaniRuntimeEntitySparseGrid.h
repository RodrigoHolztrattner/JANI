////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntimeEntitySparseGrid.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"
#include "JaniRuntimeThreadContext.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

////////////////////////////////////////////////////////////////////////////////
// Class name: EntitySparseGrid
////////////////////////////////////////////////////////////////////////////////
template <typename mType, uint32_t mBucketDimSize>
class EntitySparseGrid
{
    struct Bucket
    {
        std::array<std::array<std::optional<mType>, mBucketDimSize>, mBucketDimSize> cells;
    };

public:

    EntitySparseGrid(uint32_t _world_dim_size) : m_world_dim_size(_world_dim_size)
    {
        assert(m_world_dim_size % mBucketDimSize == 0);
        m_world_dim_size     = _world_dim_size / mBucketDimSize;
        using size_type      = typename std::allocator_traits< std::allocator<std::vector<std::unique_ptr<Bucket>>>>::size_type;
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

    const nonstd::transient_vector<mType*> InsideRect(WorldCellCoordinates _rect_coordinates_begin, WorldCellCoordinates _rect_coordinates_end) const
    {
        return std::move(InsideRectMutable(_rect_coordinates_begin, _rect_coordinates_end));
    }

    nonstd::transient_vector<mType*> InsideRectMutable(WorldCellCoordinates _rect_coordinates_begin, WorldCellCoordinates _rect_coordinates_end) const
    {
        nonstd::transient_vector<mType*> out_result;

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

        for (int x = start_x; x <= end_x; x++)
        {
            for (int y = start_y; y <= end_y; y++)
            {
                if (IsCellEmpty(WorldCellCoordinates({ x, y })))
                {
                    continue;
                }
                auto& bucket_value = AtMutable(WorldCellCoordinates({ x, y }));
                out_result.push_back(&bucket_value);
            }
        }
        return std::move(out_result);
    }

    const nonstd::transient_vector<mType*> InsideRange(WorldCellCoordinates _cell_coordinates, float _radius) const
    {
        return std::move(InsideRangeMutable(_cell_coordinates, _radius));
    }

    nonstd::transient_vector<mType*> InsideRangeMutable(WorldCellCoordinates _cell_coordinates, float _radius) const
    {
        nonstd::transient_vector<mType*> out_result;

        // TODO:
        // This calculation should be improved! It will (in most cases) lead into a 3x3 grid search when
        // it could perform the search only on the central grid
        // I need to pass the actual position inside the cell and determine if it with the radius really imply in
        // searching all the neighbors or just a few/none
        auto [bucket_x, bucket_y, local_x, local_y] = ConvertWorldToLocalCoordinates(_cell_coordinates.x, _cell_coordinates.y);
        int actual_radius      = std::ceil(_radius);
        int actual_radius_pow2 = actual_radius * actual_radius;
        int start_x            = _cell_coordinates.x - actual_radius;
        int start_y            = _cell_coordinates.y - actual_radius;
        int end_x              = _cell_coordinates.x + actual_radius;
        int end_y              = _cell_coordinates.y + actual_radius;
        
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
                out_result.push_back(&bucket_value);
            }
        }
        return std::move(out_result);
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
};


// Jani
JaniNamespaceEnd(Jani)