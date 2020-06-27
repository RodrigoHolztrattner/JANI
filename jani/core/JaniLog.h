////////////////////////////////////////////////////////////////////////////////
// Filename: JaniLog.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <mutex>
#include <string>
#include <spdlog/spdlog.h>

namespace Jani
{

    static const std::string JaniConsoleName = "jani_console";

    static void InitializeStandardConsole()
    {
        // Initialize the console logger we are going to use
        auto console = spdlog::stdout_color_mt(JaniConsoleName);
        console->set_pattern("%Y-%m-%d %H:%M:%S.%e [thread %t] [%^%l%$] : %v");
    }

    class MessageLog
    {
    public:

        MessageLog(const std::string& _log_namespace) : m_log_namespace(_log_namespace)
        {
        }
        MessageLog() : m_log_namespace(JaniConsoleName)
        {
        }

        template<typename Arg1, typename... Args>
        void Trace(const char* fmt, const Arg1& arg1, const Args& ... args)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->trace(fmt, arg1, args...);
        }

        template<typename T>
        void Trace(const T& msg)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->trace(msg);
        }

        template<typename Arg1, typename... Args>
        void Debug(const char* fmt, const Arg1& arg1, const Args& ... args)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->debug(fmt, arg1, args...);
        }

        template<typename T>
        void Debug(const T& msg)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->debug(msg);
        }

        template<typename Arg1, typename... Args>
        void Info(const char* fmt, const Arg1& arg1, const Args& ... args)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->info(fmt, arg1, args...);
        }

        template<typename T>
        void Info(const T& msg)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->info(msg);
        }

        template<typename Arg1, typename... Args>
        void Warning(const char* fmt, const Arg1& arg1, const Args& ... args)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->warn(fmt, arg1, args...);
        }

        template<typename T>
        void Warning(const T& msg)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->warn(msg);
        }

        template<typename Arg1, typename... Args>
        void Error(const char* fmt, const Arg1& arg1, const Args& ... args)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->error(fmt, arg1, args...);
            DebugBreak(); // TODO: Remove the debugbreak from any error messages, it should only break on critical
        }

        template<typename T>
        void Error(const T& msg)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->error(msg);
            DebugBreak(); // TODO: Remove the debugbreak from any error messages, it should only break on critical
        }

        template<typename Arg1, typename... Args>
        void Critical(const char* fmt, const Arg1& arg1, const Args& ... args)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->critical(fmt, arg1, args...);
            DebugBreak();
        }

        template<typename T>
        void Critical(const T& msg)
        {
            std::lock_guard<std::mutex> l(m_log_mutex);
            spdlog::get(m_log_namespace)->critical(msg);
            DebugBreak();
        }

    private:

        static std::mutex  m_log_mutex;
        const std::string& m_log_namespace;
    };
}

#define JaniTrace(fmt, ...)                                                                         \
{                                                                                                   \
    {Jani::MessageLog().Trace(fmt, ##__VA_ARGS__);}                                                 \
}

#define JaniInfo(fmt, ...)                                                                          \
{                                                                                                   \
    {Jani::MessageLog().Info(fmt, ##__VA_ARGS__);}                                                  \
}

#define JaniWarning(fmt, ...)                                                                       \
{                                                                                                   \
    {Jani::MessageLog().Warning(fmt, ##__VA_ARGS__);}                                               \
}

#define JaniError(fmt, ...)                                                                         \
{                                                                                                   \
    {Jani::MessageLog().Error(fmt, ##__VA_ARGS__);}                                                 \
}

#define JaniCritical(fmt, ...)                                                                      \
{                                                                                                   \
    {Jani::MessageLog().Critical(fmt, ##__VA_ARGS__);}                                              \
}

#define JaniLogOnce(fmt, ...)                                                                       \
{                                                                                                   \
    static std::once_flag once_flag;                                                                \
    std::call_once(once_flag, []() { Jani::MessageLog().Info(fmt, ##__VA_ARGS__); });               \
}

#define JaniWarnOnce(fmt, ...)                                                                      \
{                                                                                                   \
    static std::once_flag once_flag;                                                                \
    std::call_once(once_flag, []() { Jani::MessageLog().Warning(fmt, ##__VA_ARGS__); });            \
}

#define JaniErrorOnce(fmt, ...)                                                                     \
{                                                                                                   \
    static std::once_flag once_flag;                                                                \
    std::call_once(once_flag, []() { Jani::MessageLog().Error(fmt, ##__VA_ARGS__); });              \
}

#define JaniCriticalOnce(fmt, ...)                                                                  \
{                                                                                                   \
    static std::once_flag once_flag;                                                                \
    std::call_once(once_flag, []() { Jani::MessageLog().Critical(fmt, ##__VA_ARGS__); });           \
}

#define JaniTraceIfTrue(expression, fmt, ...)                                                       \
{                                                                                                   \
    {if((expression)) Jani::MessageLog().Trace(fmt, ##__VA_ARGS__);}                                \
}

#define JaniTraceOnFail(expression, fmt, ...)                                                       \
{                                                                                                   \
    {if(!(expression)) Jani::MessageLog().Trace(fmt, ##__VA_ARGS__);}                               \
}

#define JaniLogIfTrue(expression, fmt, ...)                                                         \
{                                                                                                   \
    {if((expression)) Jani::MessageLog().Info(fmt, ##__VA_ARGS__);}                                 \
}

#define JaniLogOnFail(expression, fmt, ...)                                                         \
{                                                                                                   \
    {if(!(expression)) Jani::MessageLog().Info(fmt, ##__VA_ARGS__);}                                \
}

#define JaniWarnOnFail(expression, fmt, ...)                                                        \
{                                                                                                   \
    {if(!(expression)) Jani::MessageLog().Warning(fmt, ##__VA_ARGS__);}                             \
}

#define JaniErrorOnFail(expression, fmt, ...)                                                       \
{                                                                                                   \
    {if(!(expression)) Jani::MessageLog().Error(fmt, ##__VA_ARGS__);}                               \
}

#define JaniCriticalOnFail(expression, fmt, ...)                                                    \
{                                                                                                   \
    {if(!(expression)) Jani::MessageLog().Critical(fmt, ##__VA_ARGS__);}                            \
}

