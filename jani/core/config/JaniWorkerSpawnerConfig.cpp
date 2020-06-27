////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorkerSpawnerConfig.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorkerSpawnerConfig.h"

Jani::WorkerSpawnerConfig::WorkerSpawnerConfig()
{
}

Jani::WorkerSpawnerConfig::~WorkerSpawnerConfig()
{
}

bool Jani::WorkerSpawnerConfig::Initialize(const std::string& _config_file_path)
{
    std::ifstream file(_config_file_path);
    if (!file.is_open())
    {
        return false;
    }

    nlohmann::json config_json;
    file >> config_json;

    auto worker_spawners = config_json.find("worker_spawners");
    if (worker_spawners == config_json.end() || !worker_spawners->is_array())
    {
        return false;
    }

    uint32_t layer_id_counter = 0;
    for (auto& worker_spawner : *worker_spawners)
    {
        if (worker_spawner.find("ip") == worker_spawner.end())
        {
            return false;
        }
        if (worker_spawner.find("port") == worker_spawner.end())
        {
            return false;
        }

        WorkerSpawnerInfo worker_spawner_info;

        worker_spawner_info.ip   = worker_spawner["ip"];
        worker_spawner_info.port = worker_spawner["port"];

        m_worker_spawners.push_back(std::move(worker_spawner_info));
    }

    m_is_valid = true;
    return true;
}

bool Jani::WorkerSpawnerConfig::IsValid() const
{
    return m_is_valid;
}

const std::vector<Jani::WorkerSpawnerConfig::WorkerSpawnerInfo>& Jani::WorkerSpawnerConfig::GetWorkerSpawnersInfos() const
{
    return m_worker_spawners;
}