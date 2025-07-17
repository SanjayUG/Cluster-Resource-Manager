#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

enum class LogLevel { INFO, WARNING, ERROR };

class Logger {
public:
    static void log(const std::string& who, const std::string& message, LogLevel level = LogLevel::INFO) {
        auto now = std::chrono::system_clock::now();
        std::time_t now_time = std::chrono::system_clock::to_time_t(now);
        std::tm* now_tm = std::localtime(&now_time);

        std::ostringstream oss;
        oss << "[" << std::put_time(now_tm, "%Y-%m-%d %H:%M:%S") << "] ";

        switch (level) {
            case LogLevel::INFO: oss << "[INFO]    "; break;
            case LogLevel::WARNING: oss << "[WARNING] "; break;
            case LogLevel::ERROR: oss << "[ERROR]   "; break;
        }

        oss << who << ": " << message;

        std::cout << oss.str() << std::endl;
    }
};

#endif
