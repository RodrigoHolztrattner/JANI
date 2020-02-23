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
    auto bridge_set_iter = _bridges.find(_layer_name_hash);
    if (bridge_set_iter == _bridges.end())
    {
        // There is no bridge for the given layer, one must be created, if possible
        return std::nullopt;
    }

    auto& bridge = bridge_set_iter->second;

    auto&    layer_name                      = bridge->GetLayerName();
    auto&    layer_load_balance_strategy     = bridge->GetLoadBalanceStrategy();
    auto     load_load_balance_strategy_bits = bridge->GetLoadBalanceStrategyFlags();
    uint32_t layer_unique_id                 = bridge->GetLayerId();

    // Maximum number of workers should be the first check
    bool has_space_for_more_workers   = !layer_load_balance_strategy.maximum_workers || layer_bridges.size() < layer_load_balance_strategy.maximum_workers.value();
    bool has_spatial_area_strategy    = load_load_balance_strategy_bits & LayerLoadBalanceStrategyBits::SpatialArea;
    bool has_minimum_area_requirement = layer_load_balance_strategy.minimum_area != std::nullopt && has_spatial_area_strategy;
    bool has_maximum_area_requirement = layer_load_balance_strategy.maximum_area != std::nullopt && has_spatial_area_strategy;

    uint32_t best_bridge_factor = std::numeric_limits<uint32_t>::max();
    uint32_t best_bridge_index  = 0;

    WorldPosition entity_world_position = _triggering_entity.GetRepresentativeWorldPosition(); nao faz sentido!preciso pegar o do worker e nao da bridge, ela nao tem!;

    for (int i = 0; i < layer_bridges.size(); i++)
    {
        auto& bridge = layer_bridges[i];
        assert(!layer_load_balance_strategy.maximum_workers || bridge->AcceptLoadBalanceStrategy(LayerLoadBalanceStrategyBits::MaximumWorkers));

        uint32_t current_bridge_points = 0;

        if (has_minimum_area_requirement)
        {
            uint32_t area = bridge->GetWorldArea();
        }

        // Distance to entity
        if (bridge->AcceptLoadBalanceStrategy(LayerLoadBalanceStrategyBits::SpatialArea))
        {
            uint32_t distance = bridge->GetDistanceToPosition(entity_world_position);

            // If it's over the maximum area, duplicate the original distance to the entity position and sum the area difference
            int32_t area_difference = bridge->GetWorldArea() - layer_load_balance_strategy.maximum_area->width * layer_load_balance_strategy.maximum_area->height;
            if (has_maximum_area_requirement && area_difference > 0)
            {
                distance *= 2;
                current_bridge_points += area_difference;
            }

            // Determine if the distance should be applied, that is true if the worker is unable to extend its area to cover the distance
            // TODO:

            current_bridge_points += distance;
        }

        if (current_bridge_points < best_bridge_factor)
        {
            best_bridge_factor = current_bridge_points;
            best_bridge_index  = i;
        }
    }

    // Check if the factor is too high that its better to create a new one
    if (best_bridge_factor > 10 && has_space_for_more_workers)
    {
        return std::nullopt;
    }

    return layer_bridges[best_bridge_index].get();
}