#include "Logger.hpp"

// std
#include <iostream>

namespace core
{

namespace
{
const char* toLabelCStr(LogLevel l) noexcept
{
    switch (l) {
        case LogLevel::FATAL: return "[FATAL]: ";
        case LogLevel::ERROR: return "[ERROR]: ";
        case LogLevel::WARN: return "[WARN]: ";
        case LogLevel::INFO: return "[INFO]: ";
        case LogLevel::DEBUG: return "[DEBUG]: ";
        case LogLevel::TRACE:
        default: return "[TRACE]: ";
    }
}
}   // namespace

void _log(LogLevel l, std::string_view str)
{
    std::cout << toLabelCStr(l) << str;
}

}   // namespace core
