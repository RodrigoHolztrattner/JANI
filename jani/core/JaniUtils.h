////////////////////////////////////////////////////////////////////////////////
// Filename: JaniUtils.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniTypes.h"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>

namespace Jani
{
    std::string pretty_bytes(uint64_t _bytes);

} // Jani

static void to_json(nlohmann::json& j, const Jani::WorldPosition& _object);
static void from_json(const nlohmann::json& j, Jani::WorldPosition& _object);

static void to_json(nlohmann::json& j, const Jani::WorldArea& _object);
static void from_json(const nlohmann::json& j, Jani::WorldArea& _object);

static void to_json(nlohmann::json& j, const Jani::WorldRect& _object);
static void from_json(const nlohmann::json& j, Jani::WorldRect& _object);