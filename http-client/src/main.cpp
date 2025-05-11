#include "http/client.hpp"
#include "logger.hpp"
#include <iostream>

int main() {
  std::cout << "Hello, World!" << std::endl;
#ifdef ENABLE_LOGGING
  // Initialize the logger
  logger::set_level(logger::Level::debug);
  logger::init("http-client", "./client_log.txt");
  logger::enable_stdout(false);
  logger::enable_stdout(true);
  logger::enable_stdout(false);
  logger::enable_stdout(true);
  LOG_INFO("Http client started");
#endif

  http::Client cli("54.217.160.10", 8080);

  auto log_fn = [](const http::Request &req, const http::Response &res) {
    LOG_INFO("REQUEST:\nMETHOD: {}\nPATH: {}\nPROTOCOL: {}\n\n",
             http::to_string(req.method), req.path, req.protocol);
    LOG_INFO("RESPONSE:\n{} - {} - {}\nBODY: {}\nHEADERS:\n", res.version,
             res.status_code, res.status_message, res.body);
    for (const auto &[header, value] : res.headers) {
      LOG_INFO("{} - {}\n", header, value);
    }
  };
  cli.set_logger(log_fn);

  auto headers = std::unordered_map<std::string, std::string>{
      {"Content-Type", "application/x-www-form-urlencoded"},
  };
  auto res = cli.Get("/api/v1/dummy", headers);

  if (res) {
    std::cout << "Response: " << res->status_code << " - "
              << res->status_message << std::endl;
    std::cout << "Body: " << res->body << std::endl;
    for (const auto &[header, value] : res->headers) {
      std::cout << header << ": " << value << std::endl;
    }
  } else {
    std::cerr << "Error: " << static_cast<int>(res.error()) << std::endl;
  }

  return 0;
}
