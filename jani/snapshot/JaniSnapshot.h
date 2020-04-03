////////////////////////////////////////////////////////////////////////////////
// Filename: JaniSnapshot.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniInternal.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

////////////////////////////////////////////////////////////////////////////////
// Class name: Snapshot
////////////////////////////////////////////////////////////////////////////////
class Snapshot
{

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Snapshot();
    ~Snapshot();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Attempt to load this snapshot from a file
    */
    bool TryLoadFromFile(const char* _file_path);

    /*
    * Attempt to load this snapshot from memory data
    */
    bool TryLoadFromMemory(const void* _data, size_t _data_size);

    /*
    * Attempt to load this snapshot from a cereal archive
    */
    bool TryLoadFromArchive(cereal::BinaryInputArchive& _archive);

    /*
    * Return the maximum world length that was set on this snapshot
    * This should be mainly used to check if the current world length matches this snapshot
    * Using a snapshot with a different world length is undefined behavior
    */
    uint32_t GetMaximumWorldLength() const;

    /*
    * Return the cell bucket size that was set on this snapshot
    * This should be mainly used to check if the current bucket size matches this snapshot
    * Using a snapshot with a different bucket size is undefined behavior
    */
    uint32_t GetCellBucketSize() const;

    /*
    * Return a vector with all entities that are inside the given cell coordinates, if any
    */
    const std::vector<ServerEntity>& GetEntitiesForCell(WorldCellCoordinates _cell_coordinates) const;

    /*
    * Return a vector with all global entities, if any
    */
    const std::vector<ServerEntity>& GetGlobalEntities() const;

////////////////////////
private: // VARIABLES //
////////////////////////

    uint32_t m_maximum_world_length = 0;
    uint32_t m_cell_bucket_size     = 0;

    std::unordered_map<WorldCellCoordinates, std::vector<ServerEntity>, WorldCellCoordinatesHasher, WorldCellCoordinatesComparator> m_cell_entities;
    std::vector<ServerEntity>                                                                                                       m_global_entities;

    std::vector<ServerEntity> m_dummy_entity_vector; // Used whenever a reference to an empty entity vector must be returned
};

// Jani
JaniNamespaceEnd(Jani)