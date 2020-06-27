////////////////////////////////////////////////////////////////////////////////
// Filename: main.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniInternal.h"
#include "JaniRuntimeDatabase.h"
#include "JaniRuntime.h"
#include "config/JaniLayerConfig.h"
#include "config/JaniDeploymentConfig.h"
#include "config/JaniWorkerSpawnerConfig.h"

inline static uint64_t s_total_allocations = 0;
inline static uint64_t s_total_allocated   = 0;

struct FrameMetrics
{
    long long time_elapsed     = 0;
    uint64_t  allocations_count = 0;
    uint64_t  memory_allocated  = 0;
};

struct FrameGroupMetrics
{
    FrameGroupMetrics(long long _required_time_elapsed, long long _warning_elapsed_time_mark)
        : m_required_time_elapsed(_required_time_elapsed)
        , m_warning_elapsed_time_mark(_warning_elapsed_time_mark)
    {}

    void PushFrameMetrics(FrameMetrics _frame_metrics)
    {
        m_recent_frame_metrics.push_back(_frame_metrics);
    }

    void OutputMetrics(long long _time_elapsed)
    {
        m_current_time_elapsed += _time_elapsed + 1;
        if (m_current_time_elapsed < m_required_time_elapsed)
        {
            return;
        }

        m_current_time_elapsed -= m_required_time_elapsed;

        long long average_time_elapsed      = 0;
        long long highest_time_elapsed      = 0;
        uint64_t  average_allocations_count = 0;
        uint64_t  highest_allocations_count = 0;
        uint64_t  average_memory_allocated  = 0;
        uint64_t  highest_memory_allocated  = 0;

        for (auto& frame_metrics : m_recent_frame_metrics)
        {
            average_time_elapsed      += frame_metrics.time_elapsed;
            average_allocations_count += frame_metrics.allocations_count;
            average_memory_allocated  += frame_metrics.memory_allocated;

            highest_time_elapsed      = std::max(highest_time_elapsed, frame_metrics.time_elapsed);
            highest_allocations_count = std::max(highest_allocations_count, frame_metrics.allocations_count);
            highest_memory_allocated  = std::max(highest_memory_allocated, frame_metrics.memory_allocated);
        }

        average_time_elapsed      = average_time_elapsed / m_recent_frame_metrics.size();
        average_allocations_count = average_allocations_count / m_recent_frame_metrics.size();
        average_memory_allocated  = average_memory_allocated / m_recent_frame_metrics.size();

        if (highest_time_elapsed > m_warning_elapsed_time_mark)
        {
            Jani::MessageLog().Warning("Runtime -> Average frame time {} average memory allocations {} average total memory allocated {} highest frame time {} highest memory allocations {} highest memory allocated {}", average_time_elapsed, average_allocations_count, average_memory_allocated, highest_time_elapsed, highest_allocations_count, highest_memory_allocated);
        }
        else
        {
            Jani::MessageLog().Info("Runtime -> Average frame time {} average memory allocations {} average total memory allocated {} highest frame time {} highest memory allocations {} highest memory allocated {}", average_time_elapsed, average_allocations_count, average_memory_allocated, highest_time_elapsed, highest_allocations_count, highest_memory_allocated);
        }

        m_recent_frame_metrics.clear();
        m_current_time_elapsed = 0;

        s_total_allocations = 0;
        s_total_allocated   = 0;
    }

    std::vector<FrameMetrics> m_recent_frame_metrics;
    long long                 m_required_time_elapsed     = 0;
    long long                 m_current_time_elapsed      = 0;
    long long                 m_warning_elapsed_time_mark = 0;
};

FrameGroupMetrics s_frame_group_metrics(1000, 5);

void* operator new(size_t _size)
{
    s_total_allocations++;
    s_total_allocated += _size;

    return malloc(_size);
}

int main(int _argc, char* _argv[])
{
    Jani::InitializeStandardConsole();

    std::unique_ptr<Jani::RuntimeDatabase> database = std::make_unique<Jani::RuntimeDatabase>();
    std::unique_ptr<Jani::Runtime>         runtime;

    {
        std::unique_ptr<Jani::LayerConfig>         layer_collection          = std::make_unique<Jani::LayerConfig>();
        std::unique_ptr<Jani::WorkerSpawnerConfig> worker_spawner_collection = std::make_unique<Jani::WorkerSpawnerConfig>();
        std::unique_ptr<Jani::DeploymentConfig>        deployment_config         = std::make_unique<Jani::DeploymentConfig>();

        if (!layer_collection->Initialize("layers_config.json"))
        {
            Jani::MessageLog().Critical("Runtime -> LayerConfig failed to initialize");

            return false;
        }

        if (!worker_spawner_collection->Initialize("worker_spawners_config.json"))
        {
            Jani::MessageLog().Critical("Runtime -> WorkerSpawnerConfig failed to initialize");

            return false;
        }

        if (!deployment_config->Initialize("deployment_config.json"))
        {
            Jani::MessageLog().Critical("Runtime -> DeploymentConfig failed to initialize");

            return false;
        }

        runtime = std::make_unique<Jani::Runtime>(
            *database, 
            *deployment_config, 
            *layer_collection,
            *worker_spawner_collection);

        if (!runtime->Initialize())
        {
            Jani::MessageLog().Critical("Runtime -> Problem initializing runtime, verify if your config file is valid");

            return false;
        }

        while (true)
        {
            std::chrono::time_point<std::chrono::steady_clock> start_time = std::chrono::steady_clock::now();

            runtime->Update();

            auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count();

            s_frame_group_metrics.PushFrameMetrics({ elapsed_time, s_total_allocations, s_total_allocated });
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1));

            s_frame_group_metrics.OutputMetrics(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count());
        }
    }

    return 0;
}