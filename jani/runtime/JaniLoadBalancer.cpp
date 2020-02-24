////////////////////////////////////////////////////////////////////////////////
// Filename: JaniLoadBalancer.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniLoadBalancer.h"
#include "JaniBridge.h"

Jani::LoadBalancer::LoadBalancer()
{
}

Jani::LoadBalancer::~LoadBalancer()
{
}

std::optional<Jani::Bridge*> Jani::LoadBalancer::TryFindValidBridgeForLayer(
    const std::map<Hash, std::unique_ptr<Bridge>>& _bridges, 
    const Entity&                                  _triggering_entity,
    Hash                                           _layer_name_hash) const
{
    return std::nullopt;
}