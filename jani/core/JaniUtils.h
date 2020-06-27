////////////////////////////////////////////////////////////////////////////////
// Filename: JaniUtils.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

//////////////
// INCLUDES //
//////////////
#include "JaniTypes.h"
#include "JaniLog.h"

#include <nlohmann/json.hpp>
#include <cstdint>
#include <string>
#include <cmath>
#include <chrono>

namespace Jani
{
    std::string pretty_bytes(uint64_t _bytes);

    class ElapsedTimeLogger
    {
    public:

        ElapsedTimeLogger(std::chrono::time_point<std::chrono::steady_clock>& _last_time_update, uint32_t _requested_minimum_time_ms, const char* _message)
            : m_last_time_update(_last_time_update)
            , m_minimum_time(_requested_minimum_time_ms)
            , m_message_string(_message)
        {
            m_begin_time = std::chrono::steady_clock::now();
        }

        ~ElapsedTimeLogger()
        {
            auto time_now                 = std::chrono::steady_clock::now();
            auto time_from_last_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_last_time_update).count();
            if (time_from_last_update_ms > m_minimum_time)
            {
                m_last_time_update = std::chrono::steady_clock::now();
                auto time_elapsed  = std::chrono::duration_cast<std::chrono::milliseconds>(time_now - m_begin_time).count();
                MessageLog().Info((std::string(m_message_string) + std::string("{}ms")).c_str(), time_elapsed);
            }
        }

    private:

        std::chrono::time_point<std::chrono::steady_clock>& m_last_time_update;
        std::chrono::time_point<std::chrono::steady_clock>  m_begin_time;
        uint32_t                                            m_minimum_time = 0;
        const char*                                         m_message_string;
    };

#define ElapsedTimeAutoLogger(message, minimum_log_time) \
    static std::chrono::time_point<std::chrono::steady_clock> last_update_timestamp = std::chrono::steady_clock::now(); \
    ElapsedTimeLogger logger(last_update_timestamp, minimum_log_time, message);

} // Jani

static void to_json(nlohmann::json& j, const Jani::WorldPosition& _object);
static void from_json(const nlohmann::json& j, Jani::WorldPosition& _object);

static void to_json(nlohmann::json& j, const Jani::WorldArea& _object);
static void from_json(const nlohmann::json& j, Jani::WorldArea& _object);

static void to_json(nlohmann::json& j, const Jani::WorldRect& _object);
static void from_json(const nlohmann::json& j, Jani::WorldRect& _object);
