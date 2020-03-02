////////////////////////////////////////////////////////////////////////////////
// Filename: JaniLayerCollection.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniConfig.h"

///////////////
// NAMESPACE //
///////////////

// Jani
JaniNamespaceBegin(Jani)

////////////////////////////////////////////////////////////////////////////////
// Class name: LayerCollection
////////////////////////////////////////////////////////////////////////////////
class LayerCollection
{
    struct LayerInfo
    {
        std::string              name;
        bool                     is_user_layer = false;
        LayerId                  unique_id     = std::numeric_limits<LayerId>::max();
        LayerLoadBalanceStrategy load_balance_strategy;
    };

    struct ComponentInfo
    {
        std::string name;
        std::string layer_name;
        LayerId     layer_unique_id = std::numeric_limits<LayerId>::max();
        ComponentId unique_id       = std::numeric_limits<ComponentId>::max();
    };

//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    LayerCollection();
    ~LayerCollection();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Initialize this collection by reading the json file indicated by the given path
    */
    bool Initialize(const std::string& _config_file_path);

    /*
    * Returns if this collection was correctly initialized, the returned result is the same as the
    * one returned by the Initialize() function
    */
    bool IsValid() const;

    /*
    * Return a map with all registered layers and their infos
    */
    const std::map<LayerId, LayerInfo>& GetLayers() const;

    /*
    * Return a map with all registered components and their infos
    */
    const std::map<ComponentId, ComponentInfo> GetComponents() const;

    /*
    * Return the layer id associated with the given component id
    */
    LayerId GetLayerIdForComponent(ComponentId _component_id) const;

    /*
    * Return if the given layer info exist
    */
    bool HasLayer(const std::string& _layer_name) const;
    bool HasLayer(LayerId _layer_id)              const;

    /*
    * Return a layer info
    * This function considers that the layer exist (call HasLayer() to check if a layer is valid) and
    * can throw if it fails
    */
    const LayerInfo& GetLayerInfo(const std::string& _layer_name) const;
    const LayerInfo& GetLayerInfo(LayerId _layer_id)              const;

    /*
    * Return a layer load balance strategy info
    * This function considers that the layer exist (call HasLayer() to check if a layer is valid) and
    * can throw if it fails
    */
    const LayerLoadBalanceStrategy& GetLayerLoadBalanceStrategyInfo(const std::string& _layer_name) const;
    const LayerLoadBalanceStrategy& GetLayerLoadBalanceStrategyInfo(LayerId _layer_id)              const;

////////////////////////
private: // VARIABLES //
////////////////////////

    bool                                 m_is_valid = false;
    std::map<LayerId, LayerInfo>         m_layers;
    std::map<ComponentId, ComponentInfo> m_components;
};

// Jani
JaniNamespaceEnd(Jani)