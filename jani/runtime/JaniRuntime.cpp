////////////////////////////////////////////////////////////////////////////////
// Filename: JaniRuntime.cpp
////////////////////////////////////////////////////////////////////////////////
#include "JaniRuntime.h"
#include "JaniBridge.h"

Jani::Runtime::Runtime(Database& _database) :
    m_database(_database)
{
}

Jani::Runtime::~Runtime()
{
}

void Jani::Runtime::Update()
{
    ReceiveIncommingUserConnections();

    ApplyLoadBalanceRequirements();

    for (auto& bridge : m_bridges)
    {
        bridge->Update();
    }
}

void Jani::Runtime::ReceiveIncommingUserConnections()
{

}

void Jani::Runtime::ApplyLoadBalanceRequirements()
{

}