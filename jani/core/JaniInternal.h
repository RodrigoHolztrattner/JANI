////////////////////////////////////////////////////////////////////////////////
// Filename: JaniInternal.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////

#include <ctti/detailed_nameof.hpp>

#include "JaniTypes.h"
#include "JaniEnums.h"
#include "JaniStructs.h"
#include "JaniClasses.h"
#include "JaniUtils.h"

#include "connection/JaniConnection.h"

#include "JaniLog.h"

/*
    TODO LIST:

    - QBI
    - Setup layer permissions on the config file
    - Check for worker/layer permissions before doing any action
    - Setup component authority when an entity or component is added
    - Setup messages for QBI updates
    - Process the QBI (query) messages (instead of sending the query, pass only the id/component id/layer id)
    - Implement authority messages
    - Implement authority handling on the worker side
    - Implement worker disconnection recovery
    - Cell lifetime (when reaches 0 entities it should be deleted after a while)
    - Worker shutdown if unused
    - Inspector: Better info, support for sending/receiving multiple messages
    - Support for sending/receiving multiple messages
    - Add a way to bypass kcp and send a message that doesn't require an ack (mainly for query requests/responses)
*/

/////////////
// DEFINES //
/////////////

#define JaniNamespaceBegin(name)                  namespace name {
#define JaniNamespaceEnd(name)                    }

#undef max
#undef min