//
// Created by fason on 28/07/25.
//

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <chrono>
#include <fstream>
#include <mutex>
#include <string>

class Logger {
public:
    enum class Level { DEBUG, INFO, WARNING, ERROR };

    static Logger& instance();

    // Log a message with a specific level
    void log(Level level, const std::string& message);

    // Set the minimum log level (logs below this level are ignored)
    void set_min_level(Level level);

private:
    Logger();
    ~Logger();

    std::ofstream m_log_file;
    std::mutex m_mutex;
    Level m_min_level = Level::DEBUG; // Default to INFO to skip DEBUG logs in production
};

// Logging macros
#define LOG_DEBUG(msg) Logger::instance().log(Logger::Level::DEBUG, std::format("[{}:{}] {}", __FILE__, __LINE__, msg))
#define LOG_INFO(msg) Logger::instance().log(Logger::Level::INFO, std::format("[{}:{}] {}", __FILE__, __LINE__, msg))
#define LOG_WARNING(msg) Logger::instance().log(Logger::Level::WARNING, std::format("[{}:{}] {}", __FILE__, __LINE__, msg))
#define LOG_ERROR(msg) Logger::instance().log(Logger::Level::ERROR, std::format("[{}:{}] {}", __FILE__, __LINE__, msg))

#endif // LOGGER_HPP
