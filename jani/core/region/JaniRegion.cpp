////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRegion.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRegion.h"

Jani::Region::Region(uint32_t _maximum_world_length, WorldCellCoordinates _cell_coordinates)
    : m_maximum_world_length(_maximum_world_length)
    , m_cell_coordinates(_cell_coordinates)
    , m_region_name(ComposeRegionName(_cell_coordinates))
    , m_is_valid(true)
{
}

Jani::Region::Region()
{
}

Jani::Region::~Region()
{
}

std::optional<Jani::Region::EntityId> Jani::Region::InsertEntityPayload(Jani::EntityPayload&& _payload, std::vector<int8_t> _custom_payload)
{
    auto current_entity_region_id = m_available_entity_id++;

    std::unique_ptr<EntityData> entity_data = std::make_unique<EntityData>();
    entity_data->entity_region_id      = current_entity_region_id;
    entity_data->entity_payload        = std::move(_payload);
    entity_data->entity_custom_payload = std::move(_custom_payload);

    m_entity_datas.push_back(std::move(entity_data));

    m_entity_id_index_map.insert({ current_entity_region_id, static_cast<uint32_t>(m_entity_datas.size() - 1) });

    return current_entity_region_id;
}

std::optional<const Jani::EntityPayload*> Jani::Region::GetEntityPayload(EntityId _region_entity_id) const
{
    auto iter = m_entity_id_index_map.find(_region_entity_id);
    if (iter == m_entity_id_index_map.end())
    {
        return nullptr;
    }

    return &m_entity_datas[iter->second]->entity_payload;
}

std::optional<Jani::EntityPayload*> Jani::Region::GetMutableEntityPayload(EntityId _region_entity_id)
{
    auto iter = m_entity_id_index_map.find(_region_entity_id);
    if (iter == m_entity_id_index_map.end())
    {
        return nullptr;
    }

    return &m_entity_datas[iter->second]->entity_payload;
}

const std::vector<std::unique_ptr<Jani::Region::EntityData>>& Jani::Region::GetEntityPayloads() const
{
    return m_entity_datas;
}

uint32_t Jani::Region::GetMaximumWorldLength() const
{
    return m_maximum_world_length;
}

Jani::WorldCellCoordinates Jani::Region::GetCellCoordinates() const
{
    return m_cell_coordinates;
}

bool Jani::Region::IsValid() const
{
    return m_is_valid;
}

const std::string Jani::Region::GetRegionName() const
{
    return m_region_name;
}

std::string Jani::Region::ComposeRegionName(Jani::WorldCellCoordinates _region_cell_coordinates)
{
    return "jani_" + std::to_string(_region_cell_coordinates.x) + "_" + std::to_string(_region_cell_coordinates.y) + "_region";
}