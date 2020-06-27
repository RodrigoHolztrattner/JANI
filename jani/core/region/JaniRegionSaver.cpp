////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRegionSaver.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRegionSaver.h"
#include "JaniRegion.h"

Jani::RegionSaver::RegionSaver(uint32_t _maximum_world_length)
    : m_maximum_world_length(_maximum_world_length)
{
}

Jani::RegionSaver::~RegionSaver()
{
}

bool Jani::RegionSaver::SaveRegion(Jani::Region& _region)
{
    if (_region.GetMaximumWorldLength() != m_maximum_world_length || !_region.IsValid())
    {
        return false;
    }

    std::ofstream os(_region.GetRegionName() + ".jr", std::ios::binary);
    {
        cereal::BinaryOutputArchive archive(os);

        try
        {
            archive(_region);
        }
        catch (...)
        {
            return false;
        }
    }

    return true;
}