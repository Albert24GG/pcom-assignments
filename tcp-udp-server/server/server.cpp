#include "server.hpp"

#include "udp_proto.hpp"
#include "util.hpp"
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <unistd.h>

Server::Server(uint16_t port) {
  // Create TCP socket
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    listen_fd_ = -1;
    throw std::runtime_error("Failed to create TCP socket");
  }

  // Create UDP socket
  udp_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
  if (udp_fd_ < 0) {
    close(listen_fd_);
    udp_fd_ = -1;
    throw std::runtime_error("Failed to create UDP socket");
  }

  // Set up the address structure
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = hton(INADDR_ANY);
  addr.sin_port = hton(port);

  // Bind the sockets
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
}

Server::~Server() {
  if (listen_fd_ >= 0) {
    close(listen_fd_);
  }
  if (udp_fd_ >= 0) {
    close(udp_fd_);
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

bool Server::handle_udp() {
  sockaddr_in addr{};
  socklen_t addr_len = sizeof(addr);
  ssize_t bytes_received =
      recvfrom(udp_fd_, udp_buffer_.data(), udp_buffer_.size(), 0,
               reinterpret_cast<sockaddr *>(&addr), &addr_len);

  if (bytes_received < 0) {
    std::cerr << "Error receiving UDP packet: " << std::strerror(errno)
              << std::endl;
    return false;
  }

  std::cout << "Recevied " << bytes_received << std::endl;

  try {
    UdpMessage::deserialize(udp_msg_, udp_buffer_.data(), bytes_received);
    return true;
  } catch (const std::invalid_argument &e) {
    std::cerr << "Error parsing UDP payload: " << e.what() << std::endl;
    return false;
  }
}

void Server::run() {
  std::vector<pollfd> fds(3);

  // Register the initial pollfds
  register_pollfd(fds, listen_fd_, POLLIN);
  register_pollfd(fds, udp_fd_, POLLIN);
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
        } else if (fds[i].fd == udp_fd_) {
          auto msg = handle_udp();
        }
      }
    }
  }
}
