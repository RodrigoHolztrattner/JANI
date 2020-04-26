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

std::unique_ptr<Jani::Region> Jani::RegionLoader::LoadRegion(Jani::WorldCellCoordinates _cell_coordinates)
{
    std::ifstream os(Jani::Region::ComposeRegionName(_cell_coordinates) + ".jr", std::ios::binary);

    if (!os.is_open() || !os.good())
    {
        return nullptr;
    }

    std::unique_ptr<Jani::Region> out_region = std::make_unique<Jani::Region>();
    {
        cereal::BinaryInputArchive archive(os);

        try
        {
            archive(*out_region);
        }
        catch (...)
        {
            return nullptr;
        }
    }

    if (out_region->GetMaximumWorldLength() != m_maximum_world_length || !out_region->IsValid())
    {
        return nullptr;
    }

    return std::move(out_region);
}