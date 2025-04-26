#pragma once

#include "tcp_proto.hpp"
#include <array>
#include <memory>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <vector>

class Client {
public:
  /**
   * @brief Construct a new Client object.
   *
   * @param id The client ID.
   *
   * @throws std::runtime_error if the socket creation fails.
   */
  explicit Client(std::string id);

  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  Client(Client &&) = default;
  Client &operator=(Client &&) = default;

  /**
   * @brief Run the client.
   * This function will connect to the server and start the main event loop,
   * listening for commands from stdin and responses from the server that are
   * handled accordingly. The function will block until the client is stopped or
   * the connection is closed.
   *
   * @param server_addr The server address to connect to.
   *
   * @throws std::runtime_error if any critical error occurs
   */
  void run(const sockaddr_in &server_addr);

  ~Client();

private:
  struct ClientCommand {
    enum class Type { SUBSCRIBE, UNSUBSCRIBE, EXIT } type;
    std::unique_ptr<std::string> topic;
  };

  void connect_to_server(const sockaddr_in &server_addr);
  auto parse_stdin_command() -> ClientCommand;
  void prepare_id_message();
  void prepare_command_message(const ClientCommand &client_command);
  void send_tcp_message();
  void fetch_tcp_response();
  void handle_tcp_response();

  int sockfd_{-1};
  std::string id_{};

  TcpMessage tcp_msg_{};
  std::vector<std::byte> tcp_msg_buffer_{TcpMessage::MAX_SERIALIZED_SIZE};

  std::array<pollfd, 2> poll_fds_{};
};
