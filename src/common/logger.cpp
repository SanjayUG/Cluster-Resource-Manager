#include "../../include/logger.hpp"
#include <fstream>
#include <iostream>
#include <ctime>
#include <iomanip>

void logMessage(const std::string& component, const std::string& message) {
    std::ofstream log("logs/" + component + ".log", std::ios::app);
    if (log.is_open()) {
        std::time_t now = std::time(nullptr);
        std::tm* t = std::localtime(&now);
        log << "[" << std::put_time(t, "%Y-%m-%d %H:%M:%S") << "] " << message << "\n";
    }
    std::cout << "[" << component << "] " << message << std::endl;
}
