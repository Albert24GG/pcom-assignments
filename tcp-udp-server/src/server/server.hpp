#pragma once

#include "subscribers_registry.hpp"
#include "tcp_proto.hpp"
#include "udp_proto.hpp"
#include <cstdint>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
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
   * @brief Run the server
   * This function will start the server and run the main event loop, listening
   * for incoming TCP and UDP packets, and handling them accordingly. This
   * function will block until the server is stopped.
   *
   * @throws std::runtime_error if any critical error occurs
   */
  void run();

private:
  void register_pollfd(int fd, short events);
  void unregister_pollfd(size_t pollfd_index);
  void handle_stdin_cmd(bool &stop);
  auto handle_udp_msg() -> std::optional<sockaddr_in>;
  void handle_tcp_request(size_t pollfd_index);
  void fetch_tcp_request(int sockfd);
  void prepare_tcp_response(const sockaddr_in &udp_sender);
  void send_tcp_message(int sockfd);
  void disconnect_client(size_t pollfd_index);

  int listen_fd_{};
  int udp_fd_{};

  std::vector<std::byte> udp_buffer_{UdpMessage::MAX_SERIALIZED_SIZE};
  UdpMessage udp_msg_{};

  std::vector<std::byte> tcp_buffer_{TcpMessage::MAX_SERIALIZED_SIZE};
  TcpMessage tcp_msg_{};

  SubscribersRegistry subscribers_registry_{};
  std::vector<pollfd> poll_fds_{3};
};
