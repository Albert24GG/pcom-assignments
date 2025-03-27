#include "logger.hpp"

#include <memory>
#include <spdlog/sinks/basic_file_sink.h>

namespace {

std::shared_ptr<spdlog::logger> global_logger{nullptr};
logger::Level global_level{logger::Level::info};

} // namespace

namespace logger {

void init(const std::filesystem::path &log_file) {
  if (global_logger) {
    throw std::runtime_error("Logger already initialized");
  }

  if (global_level != Level::off) {
    global_logger = spdlog::basic_logger_mt("router-logger", log_file);
    global_logger->set_level(
        static_cast<spdlog::level::level_enum>(global_level));
  }
}

spdlog::logger *get_instance() {
  // If the logger has not been initialized, initialize it with default values
  if (!global_logger) {
    init();
  }
  if (!global_logger) {
    return spdlog::default_logger_raw();
  } else {
    return global_logger.get();
  }
}

void set_level(Level level) {
  global_level = level;
  spdlog::set_level(static_cast<spdlog::level::level_enum>(level));
  if (global_logger) {
    global_logger->set_level(static_cast<spdlog::level::level_enum>(level));
  }
}

} // namespace logger