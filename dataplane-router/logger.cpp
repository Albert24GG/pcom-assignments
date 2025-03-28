#include "logger.hpp"

#include <memory>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

namespace {

std::shared_ptr<spdlog::logger> global_logger{nullptr};
logger::Level global_level{logger::Level::info};

} // namespace

namespace logger {

void init(const std::filesystem::path &log_file, bool disable_stdout) {
  if (global_logger) {
    throw std::runtime_error("Logger already initialized");
  }

  if (global_level != Level::off) {
    auto log_level = static_cast<spdlog::level::level_enum>(global_level);

    if (disable_stdout) {
      global_logger = spdlog::basic_logger_mt("router-logger", log_file);
    } else {
      auto console_sink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
      console_sink->set_level(log_level);

      auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
          log_file.string(), true);
      file_sink->set_level(log_level);

      global_logger = std::make_shared<spdlog::logger>(
          "router-logger", spdlog::sinks_init_list{console_sink, file_sink});
    }
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