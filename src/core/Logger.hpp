#ifndef CORE_LOGGER_HPP
#define CORE_LOGGER_HPP

// std
#include <cstdint>
#include <format>
#include <string_view>

namespace core
{

class Logger
{
public:
    enum class Level : uint8_t {
        FATAL,
        ERROR,
        WARN,
        INFO,
        DEBUG,
        TRACE,
    };

    static Logger& get() noexcept
    {
        static Logger s_instance;
        return s_instance;
    }

    // Do not use directly, use macros to allow for easy disabling
    void _log(Level l, std::string_view str);

    // Do not use directly, use macros to allow for easy disabling
    template <typename... Args>
    void _log_fmt(Level l, std::string_view fmt_str, Args&&... args)
    {
        auto str = std::vformat(fmt_str, std::make_format_args(args...));
        _log(l, str);
    }

private:
    Logger() = default;

    static const char* toLabelCStr(Logger::Level l) noexcept;
};

}   // namespace core

#define FATAL(str) core::Logger::get()._log(core::Logger::Level::FATAL, str)
#define FATAL_FMT(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::FATAL, fmt_str, __VA_ARGS__)

#define ERROR(str) core::Logger::get()._log(core::Logger::Level::ERROR, str)
#define ERROR_FMT(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::ERROR, fmt_str, __VA_ARGS__)

#define WARN(str) core::Logger::get()._log(core::Logger::Level::WARN, str)
#define WARN_FMT(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::WARN, fmt_str, __VA_ARGS__)

#define INFO(str) core::Logger::get()._log(core::Logger::Level::INFO, str)
#define INFO_FMT(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::INFO, fmt_str, __VA_ARGS__)

#define DEBUG(str) core::Logger::get()._log(core::Logger::Level::DEBUG, str)
#define DEBUG_FMT(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::DEBUG, fmt_str, __VA_ARGS__)

#define TRACE(str) core::Logger::get()._log(core::Logger::Level::TRACE, str)
#define TRACE_FMT(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::TRACE, fmt_str, __VA_ARGS__)

#endif
