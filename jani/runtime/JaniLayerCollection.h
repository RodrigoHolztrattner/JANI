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
        Hash                     layer_hash;
        uint32_t                 unique_id = std::numeric_limits<uint32_t>::max();
        LayerLoadBalanceStrategy load_balance_strategy;
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
    * Return if the given layer info exist
    */
    bool HasLayer(const std::string& _layer_name) const;
    bool HasLayer(Hash _layer_name_hash)          const;

    /*
    * Return a layer info
    * This function considers that the layer exist (call HasLayer() to check if a layer is valid) and
    * can throw if it fails
    */
    const LayerInfo& GetLayerInfo(const std::string& _layer_name) const;
    const LayerInfo& GetLayerInfo(Hash _layer_name_hash)          const;

    /*
    * Return a layer load balance strategy info
    * This function considers that the layer exist (call HasLayer() to check if a layer is valid) and
    * can throw if it fails
    */
    const LayerLoadBalanceStrategy& GetLayerLoadBalanceStrategyInfo(const std::string& _layer_name) const;
    const LayerLoadBalanceStrategy& GetLayerLoadBalanceStrategyInfo(Hash _layer_name_hash)          const;

////////////////////////
private: // VARIABLES //
////////////////////////

    bool                      m_is_valid = false;
    std::map<Hash, LayerInfo> m_layers;
};

// Jani
JaniNamespaceEnd(Jani)