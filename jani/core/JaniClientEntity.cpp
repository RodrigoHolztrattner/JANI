////////////////////////////////////////////////////////////////////////////////
// Filename: JaniClientEntity.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniClientEntity.h"

Jani::ClientEntity::ClientEntity(EntityManager& _entity_manager, uint32_t _local_id) 
    : m_entity_manager(_entity_manager)
    , m_local_id(_local_id)
{
}

Jani::ClientEntity::~ClientEntity() 
{
}

bool Jani::ClientEntity::DestroyEntity()
{
    return m_entity_manager.get().DestroyEntity(*this);
}

bool Jani::ClientEntity::UpdateInterestQuery(
    ComponentId                   _target_component_id,
    std::vector<ComponentQuery>&& _queries)
{
    return m_entity_manager.get().UpdateInterestQuery(*this, _target_component_id, std::move(_queries));
}

bool Jani::ClientEntity::ClearInterestQuery(ComponentId _target_component_id)
{
    return m_entity_manager.get().ClearInterestQuery(*this, _target_component_id);
}