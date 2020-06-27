////////////////////////////////////////////////////////////////////////////////
// Filename: JaniSnapshot.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniSnapshot.h"
#include "config/JaniDeploymentConfig.h"
#include "config/JaniLayerConfig.h"
#include "config/JaniWorkerSpawnerConfig.h"

Jani::Snapshot::Snapshot()
{
}

Jani::Snapshot::~Snapshot()
{
}

bool Jani::Snapshot::TryLoadFromFile(const char* _file_path)
{
    std::ifstream file(_file_path, std::ios::binary);

    if (!file.is_open() || !file.good())
    {
        return false;
    }

    cereal::BinaryInputArchive archive(file);
    return TryLoadFromArchive(archive);
}

bool Jani::Snapshot::TryLoadFromMemory(const void* _data, size_t _data_size)
{
    if (_data == nullptr || _data_size == 0)
    {
        return false;
    }

    std::stringstream stream(reinterpret_cast<const char*>(_data), _data_size);

    cereal::BinaryInputArchive archive(stream);
    return TryLoadFromArchive(archive);
}

bool Jani::Snapshot::TryLoadFromArchive(cereal::BinaryInputArchive& _archive)
{
    try
    {
        _archive(m_maximum_world_length);
        _archive(m_cell_bucket_size);
        // _archive(m_cell_entities);
        // _archive(m_global_entities);
    }
    catch (...)
    {
        m_maximum_world_length = 0;
        m_cell_bucket_size     = 0;

        m_cell_entities.clear();
        m_global_entities.clear();

        return false;
    }

    return true;
}

uint32_t Jani::Snapshot::GetMaximumWorldLength() const
{
    return m_maximum_world_length;
}

uint32_t Jani::Snapshot::GetCellBucketSize() const
{
    return m_cell_bucket_size;
}

const std::vector<Jani::ServerEntity>& Jani::Snapshot::GetEntitiesForCell(WorldCellCoordinates _cell_coordinates) const
{
    auto cell_iter = m_cell_entities.find(_cell_coordinates);
    if (cell_iter == m_cell_entities.end())
    {
        return m_dummy_entity_vector;
    }

    return cell_iter->second;
}

const std::vector<Jani::ServerEntity>& Jani::Snapshot::GetGlobalEntities() const
{
    return m_global_entities;
}