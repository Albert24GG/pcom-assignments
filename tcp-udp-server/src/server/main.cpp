#include "server.hpp"
#include <charconv>
#include <iostream>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <server_port>" << std::endl;
    return 1;
  }

  uint16_t server_port{};
  auto [ptr, ec] =
      std::from_chars(argv[1], argv[1] + strlen(argv[1]), server_port);
  if (ec != std::errc{}) {
    std::cerr << "Invalid server port: " << argv[1] << std::endl;
    return 1;
  }

  try {
    Server server(server_port);
    server.run();
  } catch (const std::exception &e) {
    std::cerr << "Exception occurred: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
