#include "cli.hpp"
#include "logger.hpp"

constexpr auto HOST = "63.32.125.183";
constexpr auto PORT = 8081;

int main() {

#ifdef ENABLE_LOGGING
  // Initialize the logger
  logger::set_level(logger::Level::debug);
  logger::init("http-client", "./client_log.txt");
  logger::enable_stdout(false);
  LOG_INFO("Http client started");
#endif

  Cli cli(HOST, PORT);
  cli.run();

  return 0;
}
