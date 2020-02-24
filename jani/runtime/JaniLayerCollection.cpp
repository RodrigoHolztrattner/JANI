////////////////////////////////////////////////////////////////////////////////
// Filename: JaniLayerCollection.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniLayerCollection.h"

Jani::LayerCollection::LayerCollection()
{
}

Jani::LayerCollection::~LayerCollection()
{
}

bool Jani::LayerCollection::Initialize(const std::string& _config_file_path)
{
    std::ifstream file(_config_file_path);
    if (!file.is_open())
    {
        return false;
    }

    nlohmann::json config_json;
    file >> config_json;

    auto layers = config_json.find("layers");
    if (layers == config_json.end() || !layers->is_array())
    {
        return false;
    }

    uint32_t layer_id_counter = 0;
    for (auto& layer : *layers)
    {
        if (layer.find("name") == layer.end())
        {
            return false;
        }

        LayerInfo layer_info;

        layer_info.name          = layer["name"];
        layer_info.layer_hash    = Hasher(layer_info.name);
        layer_info.is_user_layer = layer["user_layer"];
        layer_info.unique_id     = layer_id_counter++;

        if (layer.find("maximum_workers") != layer.end())
        {
            layer_info.load_balance_strategy.maximum_workers = layer["maximum_workers"].get<uint32_t>();
        }

        if (layer.find("minimum_area") != layer.end())
        {
            auto& js = layer["minimum_area"];
            js.get<WorldArea>();
            // layer_info.load_balance_strategy.minimum_area = layer["minimum_area"].get<Jani::WorldArea>();
        }

        Hash resulting_hash = layer_info.layer_hash;
        m_layers.insert({ resulting_hash, std::move(layer_info) });
    }

    m_is_valid = true;
    return m_is_valid;
}

bool Jani::LayerCollection::IsValid() const
{
    return m_is_valid;
}

const std::map<Jani::Hash, Jani::LayerCollection::LayerInfo>& Jani::LayerCollection::GetLayers() const
{
    return m_layers;
}

bool Jani::LayerCollection::HasLayer(const std::string& _layer_name) const
{
    return m_layers.find(Hasher(_layer_name)) != m_layers.end();
}

bool Jani::LayerCollection::HasLayer(Hash _layer_name_hash) const
{
    return m_layers.find(_layer_name_hash) != m_layers.end();
}

const Jani::LayerCollection::LayerInfo& Jani::LayerCollection::GetLayerInfo(const std::string& _layer_name) const
{
    assert(m_layers.find(Hasher(_layer_name)) != m_layers.end());
    return m_layers.find(Hasher(_layer_name))->second;
}

const Jani::LayerCollection::LayerInfo& Jani::LayerCollection::GetLayerInfo(Hash _layer_name_hash) const
{
    assert(m_layers.find(_layer_name_hash) != m_layers.end());
    return m_layers.find(_layer_name_hash)->second;
}

const Jani::LayerLoadBalanceStrategy& Jani::LayerCollection::GetLayerLoadBalanceStrategyInfo(const std::string& _layer_name) const
{
    assert(m_layers.find(Hasher(_layer_name)) != m_layers.end());
    return m_layers.find(Hasher(_layer_name))->second.load_balance_strategy;
}

const Jani::LayerLoadBalanceStrategy& Jani::LayerCollection::GetLayerLoadBalanceStrategyInfo(Hash _layer_name_hash) const
{
    assert(m_layers.find(_layer_name_hash) != m_layers.end());
    return m_layers.find(_layer_name_hash)->second.load_balance_strategy;
}