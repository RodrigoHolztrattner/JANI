////////////////////////////////////////////////////////////////////////////////
// Filename: JaniConfig.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "JaniTypes.h"
#include "JaniEnums.h"
#include "JaniStructs.h"
#include "JaniUtils.h"

#include <cstdint>
#include <string>
#include <optional>
#include <filesystem>
#include <functional>
#include <iostream>
#include <fstream>
#include <set>
#include <thread>
#include <limits>
#include <mutex>
#include <string>
#include <variant>
#include <bitset>
#include <mutex>
#include <entityx/entityx.h>
#include <boost/pfr.hpp>
#include <magic_enum.hpp>
#include <ctti/type_id.hpp>
#include <ctti/static_value.hpp>
#include <ctti/detailed_nameof.hpp>
#include <cereal/cereal.hpp>
#include <cereal/archives/binary.hpp>
#include <nlohmann/json.hpp>
#include <glm/glm.hpp>
#include "span.hpp"
#include "bitset_iter.h"

#include <ikcp.h> 
#undef INLINE

#include <cereal/types/string.hpp>
#include <cereal/types/vector.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/bitset.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/polymorphic.hpp>

#include "JaniConnection.h"
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