#include "client.hpp"
#include "tcp_proto.hpp"
#include "tcp_utils.hpp"
#include "token_pattern.hpp"
#include "util.hpp"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <stdexcept>
#include <unistd.h>

Client::Client(std::string id) : id_(std::move(id)) {
  sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd_ < 0) {
    throw std::runtime_error("Failed to create TCP socket");
  }

  // Disable Nagle's algorithm
  int enable = 1;
  if (setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)) <
      0) {
    close(sockfd_);
    sockfd_ = -1;
    throw std::runtime_error("Failed to set TCP_NODELAY");
  }

  // Add stdin to the pollfd list
  poll_fds_[0].fd = STDIN_FILENO;
  poll_fds_[0].events = POLLIN;

  // Add the socket to the pollfd list
  poll_fds_[1].fd = sockfd_;
  poll_fds_[1].events = POLLIN;
}

Client::~Client() {
  if (sockfd_ >= 0) {
    close(sockfd_);
  }
}

/**
 * @brief Connect to the server
 *
 * This function connects to the server using the provided address.
 *
 * @param server_addr The server address to connect to.
 *
 * @throws std::runtime_error if the connection fails
 */
void Client::connect_to_server(const sockaddr_in &server_addr) {

  if (connect(sockfd_, reinterpret_cast<const sockaddr *>(&server_addr),
              sizeof(server_addr)) < 0) {
    close(sockfd_);
    sockfd_ = -1;
    throw std::runtime_error("Failed to connect to server");
  }
}

/**
 * @brief Prepare the TCP connect request message
 *
 * This function populates the `tcp_msg_` member with the appropriate
 * TcpRequest for connecting to the server.
 * This function does not send the message, nor does it serialize the request.
 * After calling this function, the `tcp_msg_` member can be serialized and
 * transmitted.
 */
void Client::prepare_id_message() {
  // Prepare the connect request
  tcp_msg_.payload.emplace<TcpRequest>();
  auto &req_ = std::get<TcpRequest>(tcp_msg_.payload);
  req_.type = TcpRequestType::CONNECT;

  req_.payload.emplace<TcpRequestPayloadId>();
  auto &id_payload = std::get<TcpRequestPayloadId>(req_.payload);
  id_payload.set(id_.c_str(), id_.size());
}

/**
 * @brief Prepare the TCP request based on the client command
 *
 * This function populates the `tcp_msg_` member with the appropriate
 * TcpRequest.
 * This function does not send the message, nor does it serialize the request.
 * After calling this function, the `tcp_msg_` member can be serialized and
 * transmitted.
 *
 * @param cmd The client command to prepare
 */
void Client::prepare_command_message(const ClientCommand &cmd) {
  // Prepare the request
  tcp_msg_.payload.emplace<TcpRequest>();
  auto &req_ = std::get<TcpRequest>(tcp_msg_.payload);

  switch (cmd.type) {
  case ClientCommand::Type::SUBSCRIBE:
    req_.type = TcpRequestType::SUBSCRIBE;
    break;
  case ClientCommand::Type::UNSUBSCRIBE:
    req_.type = TcpRequestType::UNSUBSCRIBE;
    break;
  default:
    unreachable();
  }

  req_.payload.emplace<TcpRequestPayloadTopic>();
  auto &topic_payload = std::get<TcpRequestPayloadTopic>(req_.payload);
  topic_payload.set(cmd.topic->c_str(), cmd.topic->size());
}

/**
 * @brief Sends the TCP message to the server
 *
 * @throws TcpSocketException if the send operation fails
 */
void Client::send_tcp_message() {
  // Serialize the message
  TcpMessage::serialize(tcp_msg_, tcp_msg_buffer_.data());
  size_t msg_size = tcp_msg_.serialized_size();

  send_all(sockfd_, tcp_msg_buffer_.data(), msg_size);
}

/**
 * @brief Parse the command from stdin
 *
 * @return The parsed command
 *
 * @throws std::invalid_argument if the command is invalid
 */
auto Client::parse_stdin_command() -> ClientCommand {
  std::string command;
  std::cin >> command;

  if (command == "exit") {
    return ClientCommand{ClientCommand::Type::EXIT, nullptr};
  }

  std::string topic;
  std::cin >> topic;

  if (topic.size() > TCP_RESP_TOPIC_MAX_SIZE) {
    throw std::invalid_argument("Topic size exceeds maximum allowed size");
  }

  // Try to create a TokenPattern from the topic string
  // This helps validating the topic pattern to avoid sending invalid
  // patterns to the server
  try {
    auto pattern =
        std::make_unique<TokenPattern>(TokenPattern::from_string(topic));
  } catch (const std::invalid_argument &e) {
    std::cerr << "Error while creating TokenPattern: " << e.what() << std::endl;
    throw std::invalid_argument("Invalid topic pattern provided: " + topic);
  }

  ClientCommand client_command{};

  if (command == "subscribe") {
    client_command.type = ClientCommand::Type::SUBSCRIBE;
  } else if (command == "unsubscribe") {
    client_command.type = ClientCommand::Type::UNSUBSCRIBE;
  } else {
    throw std::invalid_argument("Unknown command: " + command);
  }
  client_command.topic = std::make_unique<std::string>(std::move(topic));

  return client_command;
}

/**
 * @brief Fetch the TCP response from the server
 *
 * @throws TcpSocketException if the receive operation fails
 * @throws std::invalid_argument if the message is invalid
 */
void Client::fetch_tcp_response() {
  uint8_t payload_type{};
  recv_all(sockfd_, reinterpret_cast<std::byte *>(&payload_type),
           sizeof(payload_type));

  if (static_cast<TcpMessageType>(payload_type) != TcpMessageType::RESPONSE) {
    throw std::invalid_argument("Invalid TCP message type: not a response");
  }

  uint16_t payload_size{};
  recv_all(sockfd_, reinterpret_cast<std::byte *>(&payload_size),
           sizeof(payload_size));
  payload_size = ntoh(payload_size);

  if (payload_size > TcpMessage::MAX_SERIALIZED_SIZE) {
    throw std::invalid_argument("Invalid TCP message: size exceeds max limit");
  }

  recv_all(sockfd_, tcp_msg_buffer_.data(), payload_size);

  tcp_msg_.payload.emplace<TcpResponse>();

  TcpResponse::deserialize(std::get<TcpResponse>(tcp_msg_.payload),
                           tcp_msg_buffer_.data(), payload_size);
}

/**
 * @brief Handle the TCP response received from the server
 *
 * This function processes the TCP response and displays the relevant.
 * information to the stdout.
 */
void Client::handle_tcp_response() {
  auto &res = std::get<TcpResponse>(tcp_msg_.payload);

  // Display the response
  std::string udp_sender_ip(16, '\0');
  {
    in_addr udp_sender_addr{
        .s_addr = res.udp_client_ip,
    };
    inet_ntop(AF_INET, &udp_sender_addr, udp_sender_ip.data(),
              udp_sender_ip.size());
  }
  uint16_t udp_sender_port = ntoh(res.udp_client_port);

  std::cout << udp_sender_ip << ":" << udp_sender_port << " - "
            << res.topic.data() << " - ";

  switch (res.payload_type()) {
  case TcpResponsePayloadType::INT:
    std::cout << "INT";
    break;
  case TcpResponsePayloadType::SHORT_REAL:
    std::cout << "SHORT_REAL";
    break;
  case TcpResponsePayloadType::FLOAT:
    std::cout << "FLOAT";
    break;
  case TcpResponsePayloadType::STRING:
    std::cout << "STRING";
    break;
  default:
    unreachable();
  }

  std::cout << " - "
            << std::visit([](auto &&arg) { return arg.to_string(); },
                          res.payload)
            << std::endl;
}

void Client::run(const sockaddr_in &server_addr) {
  connect_to_server(server_addr);
  prepare_id_message();
  try {
    send_tcp_message();
  } catch (const std::runtime_error &e) {
    throw std::runtime_error("Failed to send connect request: " +
                             std::string(e.what()));
  }

  bool stopped = false;

  while (!stopped) {
    if (poll(poll_fds_.data(), poll_fds_.size(), -1) < 0) {
      if (errno == EINTR) {
        // Interrupted by a signal, continue polling
        continue;
      }
      std::cerr << "Error in poll: " << std::strerror(errno) << std::endl;
      throw std::runtime_error("Poll error");
    }

    if (poll_fds_[0].revents & POLLIN) {

      ClientCommand command;
      try {
        command = parse_stdin_command();
      } catch (const std::invalid_argument &e) {
        std::cerr << "Error while parsing command: " << e.what() << std::endl;
        continue;
      }

      if (command.type == ClientCommand::Type::EXIT) {
        stopped = true;
        continue;
      }

      prepare_command_message(command);
      try {
        send_tcp_message();
      } catch (const std::runtime_error &e) {
        throw std::runtime_error("Failed to send request: " +
                                 std::string(e.what()));
      }

      switch (command.type) {
      case ClientCommand::Type::SUBSCRIBE:
        std::cout << "Subscribed to topic: " << *command.topic << std::endl;
        break;
      case ClientCommand::Type::UNSUBSCRIBE:
        std::cout << "Unsubscribed from topic: " << *command.topic << std::endl;
        break;
      default:
        unreachable();
      }

    } else if (poll_fds_[1].revents & POLLIN) {
      try {
        fetch_tcp_response();
      } catch (const TcpConnectionClosed &e) {
        std::cerr << "Connection closed by server: " << e.what() << std::endl;
        stopped = true;
        continue;
      } catch (const std::exception &e) {
        std::cerr << "Error while fetching TCP response: " << e.what()
                  << std::endl;
        continue;
      }
      handle_tcp_response();
    } else if (poll_fds_[1].revents & (POLLERR | POLLHUP)) {
      std::cerr << "Connection closed by server" << std::endl;
      stopped = true;
    }
  }
}
