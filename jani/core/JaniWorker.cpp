////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorker.h"

Jani::Worker::Worker()
{
}

Jani::Worker::~Worker()
{
    m_should_exit = true;
    if (m_update_thread.joinable())
    {
        m_update_thread.join();
    }
}

bool Jani::Worker::InitializeWorker(
    uint32_t    _layer,
    uint32_t    _tick_interval_ms,
    int         _receive_port,
    int         _bridge_port,
    const char* _bridge_address,
    uint32_t    _ping_delay_ms)
{
    m_layer_id         = _layer;
    m_tick_interval_ms = _tick_interval_ms;

    m_bridge_interface = std::make_unique<BridgeInterface>(_receive_port, _bridge_port, _bridge_address);

    m_update_thread = std::thread(
        [&]()
        {
            while (m_should_exit)
            {
                InternalUpdate();
            }
        });

    return true;
}

void Jani::Worker::UnpackWorkerData(std::stringstream& _data)
{
    /* stub */
}

void Jani::Worker::PackWorkerData(std::stringstream& _data)
{
    /* stub */
}

void Jani::Worker::InternalUpdate()
{
    // Pool data from the bridge interface until the time to update comes
    auto initial_time = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - initial_time).count() < m_tick_interval_ms)
    {
        m_bridge_interface->PoolIncommingData(
            [&](auto _client_hash, nonstd::span<char> _data)
            {
                std::stringstream sstream(_data.data(), _data.size());

                // Decrypt and apply this data
                UnpackWorkerData(sstream);      
            });
    }

    auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - initial_time).count();

    Update(m_layer_id, elapsed_time);

    {
        std::array<char, MaximumPacketSize> temporary_data_buffer;

        std::stringstream sstream;
        PackWorkerData(sstream);

        sstream.read(temporary_data_buffer.data(), temporary_data_buffer.size());
        bool result = m_bridge_interface->PushData(temporary_data_buffer.data(), temporary_data_buffer.size());
    }
}