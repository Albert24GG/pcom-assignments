#include "client.hpp"
#include "util.hpp"
#include <arpa/inet.h>
#include <charconv>
#include <iostream>
#include <netinet/in.h>
#include <string>

int main(int argc, char *argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: " << argv[0]
              << " <client_id> <server_ip> <server_port>" << std::endl;
    return 1;
  }
  std::string client_id = argv[1];

  in_addr_t server_ip = inet_addr(argv[2]);
  if (server_ip == static_cast<in_addr_t>(-1)) {
    std::cerr << "Invalid server IP address: " << argv[2] << std::endl;
    return 1;
  }

  uint16_t server_port{};
  auto [ptr, ec] =
      std::from_chars(argv[3], argv[3] + strlen(argv[3]), server_port);
  if (ec != std::errc{}) {
    std::cerr << "Invalid server port: " << argv[3] << std::endl;
    return 1;
  }

  sockaddr_in server_addr{};
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = hton(server_port);
  server_addr.sin_addr.s_addr = server_ip;

  try {
    Client client(client_id);
    client.run(server_addr);
  } catch (const std::exception &e) {
    std::cerr << "Exception occurred: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
