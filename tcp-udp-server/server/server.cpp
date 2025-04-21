#include "server.hpp"

#include "udp_proto.hpp"
#include "util.hpp"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

Server::Server(uint16_t port) {
  // Create TCP socket
  tcp_socket_ = socket(AF_INET, SOCK_STREAM, 0);
  if (tcp_socket_ < 0) {
    tcp_socket_ = -1;
    throw std::runtime_error("Failed to create TCP socket");
  }

  // Create UDP socket
  udp_socket_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_socket_ < 0) {
    close(tcp_socket_);
    udp_socket_ = -1;
    throw std::runtime_error("Failed to create UDP socket");
  }

  // Set up the address structure
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = hton(INADDR_ANY);
  addr.sin_port = hton(port);

  // Bind the sockets
  if (bind(tcp_socket_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
      0) {
    close(tcp_socket_);
    close(udp_socket_);
    tcp_socket_ = udp_socket_ = -1;
    throw std::runtime_error("Failed to bind TCP socket");
  }

  if (bind(udp_socket_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
      0) {
    close(tcp_socket_);
    close(udp_socket_);
    tcp_socket_ = udp_socket_ = -1;
    throw std::runtime_error("Failed to bind UDP socket");
  }
}

Server::~Server() {
  if (tcp_socket_ >= 0) {
    close(tcp_socket_);
  }
  if (udp_socket_ >= 0) {
    close(udp_socket_);
  }
}

namespace {

void register_pollfd(std::vector<pollfd> &fds, int fd, short events) {
  pollfd pfd{
      .fd = fd,
      .events = events,
  };
  fds.push_back(pfd);
}

void unregister_pollfd(std::vector<pollfd> &fds, size_t index) {

  if (fds[index].fd > 0) {
    close(fds[index].fd);
  }

  fds[index] = fds.back();
  fds.pop_back();
}

UdpPayloadVariant extract_payload(const std::byte *payload,
                                  UdpPayloadType payload_type,
                                  size_t payload_size) {
  if (payload_size < UDP_MSG_PAYLOAD_MIN_SIZE[payload_type]) {
    throw std::invalid_argument(
        "Malformed UDP payload: payload size too small");
  }
  // If payload size if greater than the required size, the rest of the payload
  // is ignored

  switch (payload_type) {
  case UdpPayloadType::INT: {
    UdpPayload<UdpPayloadType::INT> payload_int;
    memcpy(&payload_int.sign, payload, sizeof(payload_int.sign));
    payload += sizeof(payload_int.sign);
    memcpy(&payload_int.value, payload, sizeof(payload_int.value));
    payload_int.value = ntoh(payload_int.value);
    return payload_int;
  }
  case UdpPayloadType::SHORT_REAL: {
    UdpPayload<UdpPayloadType::SHORT_REAL> payload_short_real;
    memcpy(&payload_short_real.value, payload,
           sizeof(payload_short_real.value));
    payload_short_real.value = ntoh(payload_short_real.value);
    return payload_short_real;
  }
  case UdpPayloadType::FLOAT: {
    UdpPayload<UdpPayloadType::FLOAT> payload_float;
    memcpy(&payload_float.sign, payload, sizeof(payload_float.sign));
    payload += sizeof(payload_float.sign);
    memcpy(&payload_float.value, payload, sizeof(payload_float.value));
    payload_float.value = ntoh(payload_float.value);
    payload += sizeof(payload_float.value);
    memcpy(&payload_float.exponent, payload, sizeof(payload_float.exponent));
    return payload_float;
  }
  case UdpPayloadType::STRING: {
    UdpPayload<UdpPayloadType::STRING> payload_string;
    size_t str_len = strnlen(reinterpret_cast<const char *>(payload),
                             UDP_MSG_PAYLOAD_MAX_SIZE);
    payload_string.value =
        std::string(reinterpret_cast<const char *>(payload), str_len);
    return payload_string;
  }
  default:
    throw std::invalid_argument("Unknown payload type");
  }
}

} // namespace

void Server::handle_stdin(bool &stop) {
  std::string input;
  std::getline(std::cin, input);

  if (input == "exit") {
    // Stop the server
    stop = true;
    return;
  }
}

auto Server::handle_udp() -> std::optional<UdpMessage> {
  sockaddr_in addr{};
  socklen_t addr_len = sizeof(addr);
  ssize_t bytes_received =
      recvfrom(udp_socket_, udp_buffer_.data(), udp_buffer_.size(), 0,
               reinterpret_cast<sockaddr *>(&addr), &addr_len);

  if (bytes_received < 0) {
    std::cerr << "Error receiving UDP packet: " << std::strerror(errno)
              << std::endl;
    return std::nullopt;
  }

  UdpMessage msg;
  size_t offset = 0;
  {
    size_t topic_len = strnlen(reinterpret_cast<char *>(udp_buffer_.data()),
                               UDP_MSG_TOPIC_MAX_SIZE);
    msg.topic =
        std::string(reinterpret_cast<char *>(udp_buffer_.data()), topic_len);

    offset += UDP_MSG_TOPIC_MAX_SIZE;
  }
  UdpPayloadType payload_type =
      static_cast<UdpPayloadType>(udp_buffer_[offset++]); // payload type

  std::byte *payload = udp_buffer_.data() + offset;

  try {
    msg.payload =
        extract_payload(payload, payload_type, bytes_received - offset);
    return msg;
  } catch (const std::invalid_argument &e) {
    std::cerr << "Error parsing UDP payload: " << e.what() << std::endl;
    return std::nullopt;
  }
}

void Server::run() {
  std::vector<pollfd> fds(3);

  // Register the initial pollfds
  register_pollfd(fds, tcp_socket_, POLLIN);
  register_pollfd(fds, udp_socket_, POLLIN);
  register_pollfd(fds, STDIN_FILENO, POLLIN);

  bool stopped = false;

  while (!stopped) {
    if (poll(fds.data(), fds.size(), -1) == -1) {
      if (errno == EINTR) {
        // Interrupted by a signal, continue polling
        continue;
      } else {
        std::cerr << "Error in poll: " << std::strerror(errno) << std::endl;
        throw std::runtime_error("Poll error");
      }
    }

    for (size_t i = 0; i < fds.size(); ++i) {
      if (fds[i].revents & POLLIN) {
        if (fds[i].fd == STDIN_FILENO) {
          handle_stdin(stopped);
        } else if (fds[i].fd == udp_socket_) {
          auto msg = handle_udp();
        }
      }
    }
  }
}
