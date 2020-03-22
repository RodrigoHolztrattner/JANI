////////////////////////////////////////////////////////////////////////////////
// Filename: DeploymentConfig.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniDeploymentConfig.h"

Jani::DeploymentConfig::DeploymentConfig()
{
}

Jani::DeploymentConfig::~DeploymentConfig()
{
}

bool Jani::DeploymentConfig::Initialize(const std::string& _config_file_path)
{
    std::ifstream file(_config_file_path);
    if (!file.is_open())
    {
        return false;
    }

    nlohmann::json config_json;
    file >> config_json;

    if (config_json.find("uses_centralized_world_origin") == config_json.end()
        || config_json.find("maximum_world_length") == config_json.end()
        || config_json.find("worker_length") == config_json.end())
    {
        return false;
    }

    m_uses_centralized_world_origin = config_json["uses_centralized_world_origin"];
    m_maximum_world_length          = config_json["maximum_world_length"];
    m_worker_length                 = config_json["worker_length"];

    if (m_maximum_world_length % m_worker_length != 0)
    {
        return false;
    }

    m_grids_per_world_line = m_maximum_world_length / m_worker_length;

    m_is_valid = true;
    return m_is_valid;
}

bool Jani::DeploymentConfig::IsValid() const
{
    return m_is_valid;
}

uint32_t Jani::DeploymentConfig::GetMaximumWorldLength() const
{
    return m_maximum_world_length;
}

uint32_t Jani::DeploymentConfig::GetWorkerLength() const
{
    return m_worker_length;
}

uint32_t Jani::DeploymentConfig::GetTotalGridsPerWorldLine() const
{
    return m_grids_per_world_line;
}

uint32_t Jani::DeploymentConfig::GetMaximumWorkerCount() const
{
    return m_grids_per_world_line * m_grids_per_world_line;
}

bool Jani::DeploymentConfig::UsesCentralizedWorldOrigin() const
{
    return m_uses_centralized_world_origin;
}