#ifndef CORE_LOGGER_HPP
#define CORE_LOGGER_HPP

// std
#include <cstdint>
#include <format>
#include <iostream>
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
    template <typename... Args>
    void _log_fmt(Level l, std::string_view fmt_str, Args&&... args)
    {
        auto str = std::vformat(fmt_str, std::make_format_args(args...));
        std::cout << toLabelCStr(l) << str << '\n';
    }

private:
    Logger() = default;

    static const char* toLabelCStr(Logger::Level l) noexcept;
};

}   // namespace core

#define FATAL(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::FATAL, fmt_str __VA_OPT__(, ) __VA_ARGS__)

#define ERROR(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::ERROR, fmt_str __VA_OPT__(, ) __VA_ARGS__)

#define WARN(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::WARN, fmt_str __VA_OPT__(, ) __VA_ARGS__)

#define INFO(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::INFO, fmt_str __VA_OPT__(, ) __VA_ARGS__)

#define DEBUG(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::DEBUG, fmt_str __VA_OPT__(, ) __VA_ARGS__)

#define TRACE(fmt_str, ...) core::Logger::get()._log_fmt(core::Logger::Level::TRACE, fmt_str __VA_OPT__(, ) __VA_ARGS__)

#endif
