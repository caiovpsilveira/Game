#include "Logger.hpp"

namespace core
{

const char* Logger::toLabelCStr(Logger::Level l) noexcept
{
    switch (l) {
        case Logger::Level::FATAL: return "[FATAL]: ";
        case Logger::Level::ERROR: return "[ERROR]: ";
        case Logger::Level::WARN: return "[WARN]: ";
        case Logger::Level::INFO: return "[INFO]: ";
        case Logger::Level::DEBUG: return "[DEBUG]: ";
        case Logger::Level::TRACE:
        default: return "[TRACE]: ";
    }
}

}   // namespace core
