//
// Created by fason on 28/07/25.
//

#include "../include/logger.hpp"

#include <iomanip>
#include <iostream>
#include <thread>

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

Logger::Logger() {
    m_log_file.open("server.log", std::ios::app);
    if (!m_log_file.is_open()) {
        std::cerr << "Failed to open log file: server.log" << std::endl;
    }
}
Logger::~Logger() {
    if (m_log_file.is_open()) {
        m_log_file.close();
    }
}
void Logger::set_min_level(const Level level) {
    m_min_level = level;
}

void Logger::log(Level level, const std::string& message) {
    if (static_cast<int>(level) < static_cast<int>(m_min_level)) {
        return;
    }

    std::lock_guard lock(m_mutex);

    const char* level_str;
    switch (level) {
        case Level::DEBUG: level_str = "DEBUG"; break;
        case Level::INFO:  level_str = "INFO"; break;
        case Level::WARNING: level_str = "WARNING"; break;
        case Level::ERROR: level_str = "ERROR"; break;
        default: level_str = "UNKNOWN"; break;
    }

    const auto now = std::chrono::system_clock::now();
    const auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    localtime_r(&now_c, &tm);

    // Format timestamp using stringstream
    std::stringstream timestamp_ss;
    timestamp_ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    std::string timestamp = timestamp_ss.str();

    // Format the full log message
    const std::string log_message = std::format("[{}] [{}] [{}] {}",
        std::this_thread::get_id(),
        timestamp,
        level_str,
        message);

    std::cout << log_message << std::endl;
    if (m_log_file.is_open()) {
        m_log_file << log_message << std::endl;
        m_log_file.flush();
    }
}
