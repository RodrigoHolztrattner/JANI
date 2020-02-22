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
    const std::map<Hash, std::unique_ptr<LayerBridgeSet>>& _bridge_sets,
    const User&                                            _triggering_user,
    Hash                                                   _layer_name_hash) const
{
    auto bridge_set_iter = _bridge_sets.find(_layer_name_hash);
    if (bridge_set_iter == _bridge_sets.end() || bridge_set_iter->second->bridges.size() == 0)
    {
        // There is no bridge for the given layer, one must be created, if possible
        return std::nullopt;
    }

    auto& bridge_set = bridge_set_iter->second;

    auto&    layer_name                  = bridge_set->layer_name;
    auto&    layer_load_balance_strategy = bridge_set->layer_load_balance_strategy;
    uint32_t layer_unique_id             = bridge_set->layer_unique_id;
    auto&    layer_bridges               = bridge_set->bridges;

    /*
        Determinar quais tipos de load balance devem ser levados em consideracao, cada um deles deve ter um peso
        Preciso verificar primeiro se tem o que limita a quantidade e se esse limite ja foi atingido, nesse caso eu
        uso os outros parametros para desempatar
    */
    for (auto& bridge : layer_bridges)
    {
        bridge->AcceptLoadBalanceStrategy();
    }

    return std::nullopt;
}