////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRegionLoader.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRegionLoader.h"

Jani::RegionLoader::RegionLoader(uint32_t _maximum_world_length)
    : m_maximum_world_length(_maximum_world_length)
{
}

Jani::RegionLoader::~RegionLoader()
{
}

std::optional<Jani::Region> Jani::RegionLoader::LoadRegion(Jani::WorldCellCoordinates _cell_coordinates)
{
    std::ifstream os(Jani::Region::ComposeRegionName(_cell_coordinates) + ".jr", std::ios::binary);

    if (!os.is_open() || !os.good())
    {
        return std::nullopt;
    }

    Jani::Region out_region;
    {
        cereal::BinaryInputArchive archive(os);

        try
        {
            archive(out_region);
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    if (out_region.GetMaximumWorldLength() != m_maximum_world_length || !out_region.IsValid())
    {
        return std::nullopt;
    }

    return std::move(out_region);
}