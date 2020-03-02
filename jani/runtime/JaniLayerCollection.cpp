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

    auto components = config_json.find("components");
    if (components == config_json.end() || !components->is_array())
    {
        return false;
    }

    for (auto& layer : *layers)
    {
        if (layer.find("name") == layer.end())
        {
            return false;
        }

        LayerInfo layer_info;

        layer_info.name          = layer["name"];
        layer_info.unique_id     = layer["id"];
        layer_info.is_user_layer = layer["user_layer"];

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

        LayerId layer_id = layer_info.unique_id;
        m_layers.insert({ layer_id, std::move(layer_info) });
    }

    for (auto& component : *components)
    {
        if (component.find("name") == component.end())
        {
            return false;
        }

        ComponentInfo component_info;

        component_info.name       = component["name"];
        component_info.unique_id  = component["id"];
        component_info.layer_name = component["layer_name"];

        for (auto& layer : m_layers)
        {
            if (layer.second.name == component_info.layer_name)
            {
                component_info.layer_unique_id = layer.second.unique_id;
            }
        }

        if (component_info.layer_unique_id == std::numeric_limits<ComponentId>::max())
        {
            return false;
        }

        struct ComponentInfo
        {
            std::string name;
            LayerId     layer_unique_id = std::numeric_limits<LayerId>::max();
            ComponentId unique_id = std::numeric_limits<ComponentId>::max();
        };

        ComponentId component_id = component_info.unique_id;
        m_components.insert({ component_id, std::move(component_info) });
    }
    

    m_is_valid = true;
    return m_is_valid;
}

bool Jani::LayerCollection::IsValid() const
{
    return m_is_valid;
}

const std::map<Jani::LayerId, Jani::LayerCollection::LayerInfo>& Jani::LayerCollection::GetLayers() const
{
    return m_layers;
}

const std::map<Jani::ComponentId, Jani::LayerCollection::ComponentInfo> Jani::LayerCollection::GetComponents() const
{
    return m_components;
}

Jani::LayerId Jani::LayerCollection::GetLayerIdForComponent(ComponentId _component_id) const
{
    assert(m_components.find(_component_id) != m_components.end());
    return m_components.find(_component_id)->second.layer_unique_id;
}

bool Jani::LayerCollection::HasLayer(const std::string& _layer_name) const
{
    return m_layers.find(Hasher(_layer_name)) != m_layers.end();
}

bool Jani::LayerCollection::HasLayer(LayerId _layer_id) const
{
    return m_layers.find(_layer_id) != m_layers.end();
}

const Jani::LayerCollection::LayerInfo& Jani::LayerCollection::GetLayerInfo(const std::string& _layer_name) const
{
    assert(m_layers.find(Hasher(_layer_name)) != m_layers.end());
    return m_layers.find(Hasher(_layer_name))->second;
}

const Jani::LayerCollection::LayerInfo& Jani::LayerCollection::GetLayerInfo(LayerId _layer_id) const
{
    assert(m_layers.find(_layer_id) != m_layers.end());
    return m_layers.find(_layer_id)->second;
}

const Jani::LayerLoadBalanceStrategy& Jani::LayerCollection::GetLayerLoadBalanceStrategyInfo(const std::string& _layer_name) const
{
    assert(m_layers.find(Hasher(_layer_name)) != m_layers.end());
    return m_layers.find(Hasher(_layer_name))->second.load_balance_strategy;
}

const Jani::LayerLoadBalanceStrategy& Jani::LayerCollection::GetLayerLoadBalanceStrategyInfo(LayerId _layer_id) const
{
    assert(m_layers.find(_layer_id) != m_layers.end());
    return m_layers.find(_layer_id)->second.load_balance_strategy;
}