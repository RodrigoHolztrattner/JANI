////////////////////////////////////////////////////////////////////////////////
// Filename: Region.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "../JaniInternal.h"

#include <vector>
#include <cstdint>
#include <limits>
#include <memory>
#include <map>

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

////////////////////////////////////////////////////////////////////////////////
// Class name: Region
////////////////////////////////////////////////////////////////////////////////
class Region
{
public:

    /*
    * Used to identify an entity inside this region
    * This identifier cannot be used between multiple regions
    */
    using EntityId = uint32_t;

private:

    struct EntityData
    {
        Serializable();

        EntityId            entity_region_id = std::numeric_limits<EntityId>::max();
        Jani::EntityPayload entity_payload;
        std::vector<int8_t> entity_custom_payload;
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Region(uint32_t _maximum_world_length, WorldCellCoordinates _cell_coordinates);
    Region();
    ~Region();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    *
    */
    std::optional<EntityId> InsertEntityPayload(Jani::EntityPayload&& _payload, std::vector<int8_t> _custom_payload = std::vector<int8_t>());

    /*
    *
    */
    std::optional<const Jani::EntityPayload*> GetEntityPayload(EntityId _region_entity_id) const;

    /*
    *
    */
    std::optional<Jani::EntityPayload*> GetMutableEntityPayload(EntityId _region_entity_id);

    /*
    *
    */
    const std::vector<std::unique_ptr<Region::EntityData>>& GetEntityPayloads() const;

    /*
    *
    */
    uint32_t GetMaximumWorldLength() const;

    /*
    *
    */
    WorldCellCoordinates GetCellCoordinates() const;

    /*
    *
    */
    bool IsValid() const;

    /*
    *
    */
    const std::string GetRegionName() const;

    /*
    *
    */
    static std::string ComposeRegionName(Jani::WorldCellCoordinates _region_cell_coordinates);

///////////////////////////
public: // SERIALIZATION //
///////////////////////////

    template <class Archive>
    void serialize(Archive& ar)
    {
        ar(m_is_valid, 
           m_region_name,
           m_maximum_world_length,
           m_cell_coordinates, 
           m_available_entity_id, 
           m_entity_datas, 
           m_entity_id_index_map);
    }

////////////////////////
private: // VARIABLES //
////////////////////////

    bool                                     m_is_valid             = false;
    std::string                              m_region_name;
    uint32_t                                 m_maximum_world_length = 0;
    WorldCellCoordinates                     m_cell_coordinates;
    EntityId                                 m_available_entity_id  = 0;
    std::vector<std::unique_ptr<EntityData>> m_entity_datas;
    std::unordered_map<EntityId, uint32_t>   m_entity_id_index_map;
};

// Jani
JaniNamespaceEnd(Jani)