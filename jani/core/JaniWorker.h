////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorker.h
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
// Class name: Worker
////////////////////////////////////////////////////////////////////////////////
class Worker
{
//////////////////////////
public: // CONSTRUCTORS //
//////////////////////////

    Worker();
    ~Worker();

//////////////////////////
public: // MAIN METHODS //
//////////////////////////

    /*
    * Create and connects this worker with its runtime server
    * Must be called in order to start operating as a worker
    */
    bool InitializeWorker(
        uint32_t    _layer, 
        uint32_t    _tick_interval_ms, 
        int         _receive_port, 
        int         _bridge_port,
        const char* _bridge_address, 
        uint32_t    _ping_delay_ms);

public:

    /*
    * The main update function
    * Used this to process current component data
    */
    virtual void Update(uint32_t _layer_id, uint32_t _time_elapsed_ms) = 0;

    /*
    *
    */
    virtual void UnpackWorkerData(std::stringstream& _data);

    /*
    *
    */
    virtual void PackWorkerData(std::stringstream& _data);

private:

    void InternalUpdate();

////////////////////////
private: // VARIABLES //
////////////////////////

    // The bridge interface
    std::unique_ptr<BridgeInterface> m_bridge_interface;

    // Represents the layer id this worker is working at
    uint32_t m_layer_id = std::numeric_limits<uint32_t>::max();

    uint32_t m_tick_interval_ms = 0;

    std::thread m_update_thread;

    bool m_should_exit = false;
};

// Jani
JaniNamespaceEnd(Jani)