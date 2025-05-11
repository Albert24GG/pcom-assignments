#pragma once

#ifndef ENABLE_LOGGING

#define LOG_TRACE(...) (void)0
#define LOG_DEBUG(...) (void)0
#define LOG_INFO(...) (void)0
#define LOG_WARN(...) (void)0
#define LOG_ERROR(...) (void)0
#define LOG_CRITICAL(...) (void)0

#else

#ifdef DEBUG
#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#endif
#include <filesystem>
#include <spdlog/spdlog.h>

#define LOG_TRACE(...) SPDLOG_LOGGER_TRACE(logger::get_instance(), __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_DEBUG(logger::get_instance(), __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_INFO(logger::get_instance(), __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_WARN(logger::get_instance(), __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_ERROR(logger::get_instance(), __VA_ARGS__)
#define LOG_CRITICAL(...)                                                      \
  SPDLOG_LOGGER_CRITICAL(logger::get_instance(), __VA_ARGS__)

namespace logger {

enum class Level {
  trace = SPDLOG_LEVEL_TRACE,
  debug = SPDLOG_LEVEL_DEBUG,
  info = SPDLOG_LEVEL_INFO,
  warn = SPDLOG_LEVEL_WARN,
  error = SPDLOG_LEVEL_ERROR,
  critical = SPDLOG_LEVEL_CRITICAL,
  off = SPDLOG_LEVEL_OFF
};

/**
 * @brief Initialize the logger with the given log directory and log name
 *
 * @param logger_name The name of the logger
 * @param log_file The path to the log file
 */
void init(const std::string &logger_name = "logger",
          const std::filesystem::path &log_file = "./log.txt");

/**
 * @brief Get the instance of the logger
 *
 * @return The instance of the logger
 */
spdlog::logger *get_instance();

/**
 * @brief Set the level of the logger
 *
 * @param level The level to set the logger to
 */
void set_level(Level level);

/**
 * @brief Enable or disable stdout logging
 *
 * @param enable If true, enable stdout logging. If false, disable it.
 */
void enable_stdout(bool enable = true);

}; // namespace logger

#endif // ENABLE_LOGGING
