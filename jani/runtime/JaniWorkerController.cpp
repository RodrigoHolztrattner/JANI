////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorkerController.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorkerController.h"
#include "JaniDeploymentConfig.h"
#include "JaniLayerCollection.h"

Jani::LoadBalancer::LoadBalancer(
    const DeploymentConfig& _deployment_config,
    const LayerCollection&  _layer_collection) 
    : m_deployment_config(_deployment_config)
    , m_layer_collection(_layer_collection)
{

}

Jani::LoadBalancer::~LoadBalancer()
{
}

bool Jani::LoadBalancer::Initialize()
{
    if (!m_deployment_config.IsValid()
        || !m_layer_collection.IsValid())
    {
        return false;
    }

    auto& layer_collection_infos = m_layer_collection.GetLayers();
    LayerId layer_id_count = 0;
    for (auto& [layer_id, layer_collection_info] : layer_collection_infos)
    {
        auto& layer_info = m_layer_infos[layer_id];

        layer_info.user_layer                  = layer_collection_info.user_layer;
        layer_info.uses_spatial_area           = layer_collection_info.use_spatial_area;
        layer_info.maximum_entities_per_worker = layer_collection_info.maximum_entities_per_worker;
        layer_info.maximum_workers             = layer_collection_info.maximum_workers;
        layer_info.layer_id                    = layer_id_count++;
    }

    return true;
}

std::optional<Jani::Bridge*> Jani::LoadBalancer::TryFindValidBridgeForLayer(
    const std::map<Hash, std::unique_ptr<Bridge>>& _bridges, 
    const Entity&                                  _triggering_entity,
    Hash                                           _layer_name_hash) const
{
    return std::nullopt;
}