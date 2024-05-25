#ifndef CORE_LOGGER_HPP
#define CORE_LOGGER_HPP

// std
#include <cstdint>
#include <format>
#include <string_view>

namespace core
{

enum class LogLevel : uint8_t {
    FATAL,
    ERROR,
    WARN,
    INFO,
    DEBUG,
    TRACE,
};

// Do not use directly, use macros to allow for easy disabling and refactor
void _log(LogLevel l, std::string_view str);

// Do not use directly, use macros to allow for easy disabling and refactor
template <typename... Args>
void _log_fmt(LogLevel l, std::string_view fmt_str, Args&&... args)
{
    auto str = std::vformat(fmt_str, std::make_format_args(args...));
    _log(l, str);
}

}   // namespace core

#define FATAL(str) core::_log(core::LogLevel::FATAL, str)
#define FATAL_FMT(fmt_str, ...) core::_log_fmt(core::LogLevel::FATAL, fmt_str, __VA_ARGS__)

#define ERROR(str) core::_log(core::LogLevel::ERROR, str)
#define ERROR_FMT(fmt_str, ...) core::_log_fmt(core::LogLevel::ERROR, fmt_str, __VA_ARGS__)

#define WARN(str) core::_log(core::LogLevel::WARN, str)
#define WARN_FMT(fmt_str, ...) core::_log_fmt(core::LogLevel::WARN, fmt_str, __VA_ARGS__)

#define INFO(str) core::_log(core::LogLevel::INFO, str)
#define INFO_FMT(fmt_str, ...) core::_log_fmt(core::LogLevel::INFO, fmt_str, __VA_ARGS__)

#define DEBUG(str) core::_log(core::LogLevel::DEBUG, str)
#define DEBUG_FMT(fmt_str, ...) core::_log_fmt(core::LogLevel::DEBUG, fmt_str, __VA_ARGS__)

#define TRACE(str) core::_log(core::LogLevel::TRACE, str)
#define TRACE_FMT(fmt_str, ...) core::_log_fmt(core::LogLevel::TRACE, fmt_str, __VA_ARGS__)

#endif
