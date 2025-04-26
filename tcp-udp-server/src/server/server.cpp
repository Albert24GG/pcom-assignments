#include "server.hpp"

#include "tcp_proto.hpp"
#include "tcp_utils.hpp"
#include "udp_proto.hpp"
#include "util.hpp"
#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

using namespace std::literals;

Server::Server(uint16_t port) {
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    listen_fd_ = -1;
    throw std::runtime_error("Failed to create TCP socket");
  }

  udp_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd_ < 0) {
    close(listen_fd_);
    udp_fd_ = -1;
    throw std::runtime_error("Failed to create UDP socket");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = hton(INADDR_ANY);
  addr.sin_port = hton(port);

  if (bind(listen_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    close(listen_fd_);
    close(udp_fd_);
    listen_fd_ = udp_fd_ = -1;
    throw std::runtime_error("Failed to bind TCP socket");
  }

  if (bind(udp_fd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
    close(listen_fd_);
    close(udp_fd_);
    listen_fd_ = udp_fd_ = -1;
    throw std::runtime_error("Failed to bind UDP socket");
  }

  if (listen(listen_fd_, 0) < 0) {
    close(listen_fd_);
    close(udp_fd_);
    listen_fd_ = udp_fd_ = -1;
    throw std::runtime_error("Failed to listen on TCP socket");
  }

  // Register the initial pollfds
  register_pollfd(listen_fd_, POLLIN);
  register_pollfd(udp_fd_, POLLIN);
  register_pollfd(STDIN_FILENO, POLLIN);
}

Server::~Server() {
  if (listen_fd_ >= 0) {
    close(listen_fd_);
  }
  if (udp_fd_ >= 0) {
    close(udp_fd_);
  }
}

/**
 * @brief Register a pollfd with the server
 *
 * This function adds a new pollfd to the poll_fds_ vector.
 *
 * @param fd The file descriptor to register
 * @param events The events to monitor (e.g., POLLIN, POLLOUT)
 */
void Server::register_pollfd(int fd, short events) {
  pollfd pfd{
      .fd = fd,
      .events = events,
  };
  poll_fds_.push_back(pfd);
}

/**
 * @brief Unregister a pollfd from the server
 *
 * This function closes the file descriptor and removes it from the poll_fds_
 * vector.
 *
 * @param pollfd_index The index of the pollfd to unregister
 */
void Server::unregister_pollfd(size_t pollfd_index) {

  if (poll_fds_[pollfd_index].fd > 0) {
    close(poll_fds_[pollfd_index].fd);
  }

  poll_fds_[pollfd_index] = poll_fds_.back();
  poll_fds_.pop_back();
}

/**
 * @brief Handle commands from stdin
 *
 * At the moment, it only handles the "exit" command to stop the server, but the
 * functionality could be extended.
 *
 * @param stop A reference to a boolean that indicates whether the server should
 * stop
 */
void Server::handle_stdin_cmd(bool &stop) {
  std::string input;
  std::cin >> input;

  if (input == "exit") {
    // Stop the server
    stop = true;
    return;
  }
}

/**
 * @brief Handle an incoming UDP packet (message)
 *
 * @return std::optional<sockaddr_in> The address of the sender, or std::nullopt
 * if an error occurred or the packet was invalid
 */
auto Server::handle_udp_msg() -> std::optional<sockaddr_in> {
  sockaddr_in addr{};
  socklen_t addr_len = sizeof(addr);
  ssize_t bytes_received =
      recvfrom(udp_fd_, udp_buffer_.data(), udp_buffer_.size(), 0,
               reinterpret_cast<sockaddr *>(&addr), &addr_len);

  if (bytes_received < 0) {
    std::cerr << "Error receiving UDP packet: " << std::strerror(errno)
              << std::endl;
    return std::nullopt;
  }

  try {
    UdpMessage::deserialize(udp_msg_, udp_buffer_.data(), bytes_received);
    return addr;
  } catch (const std::invalid_argument &e) {
    std::cerr << "Error deserializing UDP payload: " << e.what() << std::endl;
    return std::nullopt;
  }
}

/**
 * @brief Prepare the TCP response based on the UDP message
 *
 * This function populates the `tcp_msg_` member with the appropriate
 * TcpResponse.
 * This function does not send the message, nor does it serialize the response.
 * After calling this function, the `tcp_msg_` member can be serialized and
 * transmitted.
 *
 * @param udp_sender_addr The address of the UDP sender
 */
void Server::prepare_tcp_response(const sockaddr_in &udp_sender_addr) {
  // Prepare the TCP response
  tcp_msg_.payload.emplace<TcpResponse>();
  auto &response = std::get<TcpResponse>(tcp_msg_.payload);

  response.udp_client_ip = udp_sender_addr.sin_addr.s_addr;
  response.udp_client_port = udp_sender_addr.sin_port;

  // Set the topic
  response.topic_size = udp_msg_.topic_size;
  std::memcpy(response.topic.data(), udp_msg_.topic.data(),
              udp_msg_.topic_size);

  // Set the payload
  switch (udp_msg_.payload_type()) {
  case UdpPayloadType::INT: {
    response.payload.emplace<TcpResponsePayloadInt>();

    auto &udp_payload = std::get<UdpPayloadInt>(udp_msg_.payload);
    auto &tcp_payload = std::get<TcpResponsePayloadInt>(response.payload);

    tcp_payload.sign = udp_payload.sign;
    tcp_payload.value = udp_payload.value;

    break;
  }
  case UdpPayloadType::SHORT_REAL: {
    response.payload.emplace<TcpResponsePayloadShortReal>();

    auto &udp_payload = std::get<UdpPayloadShortReal>(udp_msg_.payload);
    auto &tcp_payload = std::get<TcpResponsePayloadShortReal>(response.payload);

    tcp_payload.value = udp_payload.value;
    break;
  }
  case UdpPayloadType::FLOAT: {
    response.payload.emplace<TcpResponsePayloadFloat>();

    auto &udp_payload = std::get<UdpPayloadFloat>(udp_msg_.payload);
    auto &tcp_payload = std::get<TcpResponsePayloadFloat>(response.payload);

    tcp_payload.sign = udp_payload.sign;
    tcp_payload.value = udp_payload.value;
    tcp_payload.exponent = udp_payload.exponent;
    break;
  }
  case UdpPayloadType::STRING: {
    response.payload.emplace<TcpResponsePayloadString>();

    auto &udp_payload = std::get<UdpPayloadString>(udp_msg_.payload);
    auto &tcp_payload = std::get<TcpResponsePayloadString>(response.payload);

    tcp_payload.value_size = udp_payload.value_size;
    std::memcpy(tcp_payload.value.data(), udp_payload.value.data(),
                udp_payload.value_size);
    break;
  }
  default:
    unreachable();
    break;
  }
}

/**
 * @brief Fetch the TCP request from the socket
 *
 * @param sockfd The socket file descriptor of the client
 *
 * @throws TcpSocketException if the receive operation fails
 * @throws std::invalid_argument if the message is invalid
 */
void Server::fetch_tcp_request(int sockfd) {
  uint8_t payload_type{};
  recv_all(sockfd, reinterpret_cast<std::byte *>(&payload_type),
           sizeof(payload_type));

  if (static_cast<TcpMessageType>(payload_type) != TcpMessageType::REQUEST) {
    throw std::invalid_argument("Invalid TCP message type: not a request");
  }

  uint16_t payload_size{};
  recv_all(sockfd, reinterpret_cast<std::byte *>(&payload_size),
           sizeof(payload_size));
  payload_size = ntoh(payload_size);

  if (payload_size > TcpMessage::MAX_SERIALIZED_SIZE) {
    throw std::invalid_argument("Invalid TCP message: size exceeds max limit");
  }

  recv_all(sockfd, tcp_buffer_.data(), payload_size);

  tcp_msg_.payload.emplace<TcpRequest>();

  TcpRequest::deserialize(std::get<TcpRequest>(tcp_msg_.payload),
                          tcp_buffer_.data(), payload_size);
}

/**
 * @brief Disconnect the client and unregister the pollfd
 *
 * @param pollfd_index The index of the pollfd associated with the client in the
 * poll_fds_ vector
 */
void Server::disconnect_client(size_t pollfd_index) {
  int sockfd = poll_fds_[pollfd_index].fd;
  if (subscribers_registry_.is_subscriber_connected(sockfd)) {
    subscribers_registry_.disconnect_subscriber(sockfd);
  }
  unregister_pollfd(pollfd_index);
}

/**
 * @brief Handle the TCP request from the client
 *
 * @param pollfd_index The index of the pollfd associated with the client in the
 * poll_fds_ vector
 */
void Server::handle_tcp_request(size_t pollfd_index) {
  int sockfd = poll_fds_[pollfd_index].fd;

  // Use a scope guard to ensure proper cleanup and avoid code duplication
  auto guard = make_scope_guard(
      std::bind(&Server::disconnect_client, this, pollfd_index));

  auto &request = std::get<TcpRequest>(tcp_msg_.payload);
  switch (request.type) {
  case TcpRequestType::CONNECT: {

    if (request.payload_type() != TcpRequestPayloadType::ID) {
      std::cerr << "Invalid payload type for CONNECT request" << std::endl;
      return;
    }

    if (subscribers_registry_.is_subscriber_connected(sockfd)) {
      std::cerr << "Invalid CONNECT request: subscriber already connected"
                << std::endl;
      return;
    }

    auto &id_payload = std::get<TcpRequestPayloadId>(request.payload);
    auto id = std::string(id_payload.id.data(), id_payload.id_size);

    try {
      subscribers_registry_.connect_subscriber(sockfd, id);
      guard.dismiss();
      sockaddr_in addr{};
      socklen_t addr_len = sizeof(addr);
      getpeername(sockfd, reinterpret_cast<sockaddr *>(&addr), &addr_len);
      std::cout << "New client " << id << " connected from "
                << inet_ntoa(addr.sin_addr) << ":" << addr.sin_port << '.'
                << std::endl;

    } catch (const std::exception &e) {
      std::cout << "Client " << id << " already connected." << std::endl;
      return;
    }
    break;
  }
  case TcpRequestType::SUBSCRIBE:
  case TcpRequestType::UNSUBSCRIBE: {
    const bool isSubscribe = request.type == TcpRequestType::SUBSCRIBE;
    std::string_view actionName = isSubscribe ? "SUBSCRIBE"sv : "UNSUBSCRIBE"sv;

    if (request.payload_type() != TcpRequestPayloadType::TOPIC) {
      std::cerr << "Invalid payload type for " << actionName << " request"
                << std::endl;
      return;
    }

    if (!subscribers_registry_.is_subscriber_connected(sockfd)) {
      std::cerr << "Invalid " << actionName
                << " request: subscriber not connected" << std::endl;
      return;
    }

    auto &topic_payload = std::get<TcpRequestPayloadTopic>(request.payload);
    std::string_view topic_str(topic_payload.topic.data(),
                               topic_payload.topic_size);

    try {
      auto topic_pat = TokenPattern::from_string(topic_str);

      if (isSubscribe) {
        subscribers_registry_.subscribe_to_topic(sockfd, topic_pat);
      } else {
        subscribers_registry_.unsubscribe_from_topic(sockfd, topic_pat);
      }

      guard.dismiss();
    } catch (const std::exception &e) {
      std::cerr << "Error "
                << (isSubscribe ? "subscribing to" : "unsubscribing from")
                << " topic: " << e.what() << std::endl;
      return;
    }

    break;
  }

  default:
    std::cerr << "Invalid request type" << std::endl;
    return;
  }
}

/**
 * @brief Send the TCP message to the client
 *
 * @param sockfd The socket file descriptor of the client
 *
 * @throws TcpSocketException if the send operation fails
 */
void Server::send_tcp_message(int sockfd) {
  // Serialize the TCP message
  TcpMessage::serialize(tcp_msg_, tcp_buffer_.data());
  size_t msg_size = tcp_msg_.serialized_size();

  send_all(sockfd, tcp_buffer_.data(), msg_size);
}

void Server::run() {
  bool stopped = false;

  while (!stopped) {
    if (poll(poll_fds_.data(), poll_fds_.size(), -1) == -1) {
      if (errno == EINTR) {
        // Interrupted by a signal, continue polling
        continue;
      } else {
        std::cerr << "Error in poll: " << std::strerror(errno) << std::endl;
        throw std::runtime_error("Poll error");
      }
    }

    // Check STDIN_FILENO
    if (poll_fds_[2].revents & POLLIN) {
      handle_stdin_cmd(stopped);
    }

    // Check udp_fd_
    if (poll_fds_[1].revents & POLLIN) {
      do {
        auto msg = handle_udp_msg();

        if (!msg.has_value()) {
          continue;
        }

        TokenPattern topic;
        std::unordered_set<int> subscribers;
        try {
          std::string_view topic_str(udp_msg_.topic.data(),
                                     udp_msg_.topic_size);
          topic = TokenPattern::from_string(topic_str);
          subscribers = subscribers_registry_.retrieve_topic_subscribers(topic);
        } catch (const std::exception &e) {
          std::cerr << "Invalid topic pattern: " << e.what() << std::endl;
          continue;
        }

        if (subscribers.empty()) {
          continue;
        }
        auto &udp_sender_addr = msg.value();
        prepare_tcp_response(udp_sender_addr);

        for (auto &sub_sockfd : subscribers) {
          if (sub_sockfd < 0) {
            continue;
          }

          // Send the TCP message to the subscriber
          try {
            send_tcp_message(sub_sockfd);
          } catch (const TcpConnectionClosed &e) {
            std::cerr << "Failed to send TCP message. Client "
                      << subscribers_registry_.get_subscriber_id(sub_sockfd)
                      << " disconnected." << std::endl;
            continue;
          } catch (const TcpSocketException &e) {
            std::cerr << "Error sending TCP message: " << e.what() << std::endl;
            continue;
          }
        }
      } while (false);
    }

    // Check listen_fd_
    if (poll_fds_[0].revents & POLLIN) {
      do {
        // Accept a new TCP connection
        int client_fd = accept(listen_fd_, nullptr, nullptr);
        if (client_fd < 0) {
          std::cerr << "Error accepting TCP connection: "
                    << std::strerror(errno) << std::endl;
          continue;
        }

        // Disable Nagle's algorithm for the TCP client
        int enable = 1;
        if (setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &enable,
                       sizeof(enable)) < 0) {
          std::cerr << "Error setting TCP_NODELAY: " << std::strerror(errno)
                    << std::endl;
          close(client_fd);
          continue;
        }

        // Add the new client fd to the pollfds
        register_pollfd(client_fd, POLLIN);
      } while (false);
    }

    // Check other pollfds (subscribers)
    for (size_t i = 3; i < poll_fds_.size();) {
      size_t initial_size = poll_fds_.size();

      do {
        if (poll_fds_[i].revents & POLLIN) {
          int sockfd = poll_fds_[i].fd;

          try {
            fetch_tcp_request(sockfd);
          } catch (const TcpConnectionClosed &e) {
            if (subscribers_registry_.is_subscriber_connected(sockfd)) {
              std::cout << "Client "
                        << subscribers_registry_.get_subscriber_id(sockfd)
                        << " disconnected." << std::endl;
            }
            disconnect_client(i);
            continue;
          } catch (const std::exception &e) {
            std::cerr << "Error while fetching TCP request: " << e.what()
                      << std::endl;
            continue;
          }

          handle_tcp_request(i);
        } else if (poll_fds_[i].revents & (POLLERR | POLLHUP)) {
          int sockfd = poll_fds_[i].fd;

          std::cout << "Client "
                    << subscribers_registry_.get_subscriber_id(sockfd)
                    << " disconnected." << std::endl;

          disconnect_client(i);
          continue;
        }
      } while (false);

      // If we removed the current pollfd, we should not increment the index
      if (initial_size <= poll_fds_.size()) {
        ++i;
      }
    }
  }
}
