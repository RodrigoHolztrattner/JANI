////////////////////////////////////////////////////////////////////////////////
// Filename: JaniLayerConfig.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniLayerConfig.h"

Jani::LayerConfig::LayerConfig()
{
}

Jani::LayerConfig::~LayerConfig()
{
}

bool Jani::LayerConfig::Initialize(const std::string& _config_file_path)
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
        if (layer.find("name") == layer.end()
            || layer.find("id") == layer.end()
            || layer.find("user_layer") == layer.end()
            || layer.find("use_spatial_area") == layer.end())
        {
            return false;
        }

        LayerInfo layer_info;

        layer_info.name             = layer["name"];
        layer_info.unique_id        = layer["id"];
        layer_info.user_layer       = layer["user_layer"];
        layer_info.use_spatial_area = layer["use_spatial_area"];

        if (layer.find("maximum_entities_per_worker") != layer.end())
        {
            layer_info.maximum_entities_per_worker = layer["maximum_entities_per_worker"].get<uint32_t>();
        }

        if (layer.find("maximum_workers") != layer.end())
        {
            layer_info.maximum_workers = layer["maximum_workers"].get<uint32_t>();
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

        if (component.find("attributes") != component.end())
        {
            auto& component_attributes = component["attributes"];
            for (auto& [attribute_name, attribute_type] : component_attributes.items())
            {
                std::string name = attribute_name;
                std::string type = attribute_type;

                bool found = false;
                for (int i = 0; i < magic_enum::enum_count<ComponentAttributeType>(); i++)
                {
                    if (type == magic_enum::enum_name<ComponentAttributeType>(magic_enum::enum_value<ComponentAttributeType>(i)))
                    {
                        component_info.component_attributes.push_back({ magic_enum::enum_value<ComponentAttributeType>(i), std::move(name) });
                        found = true;
                        break;
                    }
                }

                if (!found)
                {
                    return false;
                }
            }
        }

        for (auto& layer : m_layers)
        {
            if (layer.second.name == component_info.layer_name)
            {
                component_info.layer_unique_id = layer.second.unique_id;
                layer.second.components.insert(component_info.unique_id);

                break;
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

bool Jani::LayerConfig::IsValid() const
{
    return m_is_valid;
}

const std::map<Jani::LayerId, Jani::LayerConfig::LayerInfo>& Jani::LayerConfig::GetLayers() const
{
    return m_layers;
}

const std::map<Jani::ComponentId, Jani::LayerConfig::ComponentInfo> Jani::LayerConfig::GetComponents() const
{
    return m_components;
}

Jani::LayerId Jani::LayerConfig::GetLayerIdForComponent(ComponentId _component_id) const
{
    assert(m_components.find(_component_id) != m_components.end());
    return m_components.find(_component_id)->second.layer_unique_id;
}

bool Jani::LayerConfig::HasLayer(const std::string& _layer_name) const
{
    return m_layers.find(Hasher(_layer_name)) != m_layers.end();
}

bool Jani::LayerConfig::HasLayer(LayerId _layer_id) const
{
    return m_layers.find(_layer_id) != m_layers.end();
}

const Jani::LayerConfig::LayerInfo& Jani::LayerConfig::GetLayerInfo(const std::string& _layer_name) const
{
    assert(m_layers.find(Hasher(_layer_name)) != m_layers.end());
    return m_layers.find(Hasher(_layer_name))->second;
}

const Jani::LayerConfig::LayerInfo& Jani::LayerConfig::GetLayerInfo(LayerId _layer_id) const
{
    assert(m_layers.find(_layer_id) != m_layers.end());
    return m_layers.find(_layer_id)->second;
}