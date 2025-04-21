#pragma once

#include "udp_proto.hpp"
#include <cstdint>
#include <optional>
#include <vector>

class Server {
public:
  /**
   * @brief Construct a new Server object
   *
   * @param port The port to bind the server to
   *
   * @throws std::runtime_error if the socket creation or binding fails
   */
  explicit Server(uint16_t port);

  /**
   * @brief Destroy the Server object
   *
   * Closes the TCP and UDP sockets
   */
  ~Server();

  /**
   * @brief Start the server
   *
   * This function will start the server, listening for incoming TCP and UDP and
   * handling the packets. This will block until the server is stopped.
   */
  void run();

private:
  void handle_stdin(bool &stop);
  auto handle_udp() -> std::optional<UdpMessage>;

  int tcp_socket_{};
  int udp_socket_{};

  std::vector<std::byte> udp_buffer_{UDP_MSG_MAX_SIZE};
};
