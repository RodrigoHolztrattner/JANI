////////////////////////////////////////////////////////////////////////////////
// Filename: JaniWorkerInstance.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniWorkerInstance.h"

Jani::WorkerInstance::WorkerInstance(std::string _worker_address, uint32_t _worker_port, uint32_t _local_port, uint32_t _layer, bool _is_user_instance)
{
    m_worker_connection = std::make_unique<Connection>(0, _local_port, _worker_port, _worker_address);

    // The constructor should create a Connection object and send an initial data
    // ...
}

Jani::WorkerInstance::~WorkerInstance()
{

}

void Jani::WorkerInstance::Update()
{

}