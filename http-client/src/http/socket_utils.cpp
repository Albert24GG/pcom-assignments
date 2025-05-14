#include "socket_utils.hpp"

#include "error.hpp"
#include "scope_guard.hpp"
#include "socket.hpp"
#include "utils.hpp"
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <netdb.h>
#include <optional>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace {
using namespace http::detail;
using namespace http::utils;

auto get_host_addrinfo(const std::string &host, uint16_t port)
    -> std::optional<struct addrinfo> {
  struct addrinfo hints{};
  std::memset(&hints, 0, sizeof(hints));

  // IPv4 or IPv6
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  struct addrinfo *result;
  int status =
      getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
  if (status != 0) {
    return std::nullopt;
  }
  auto host_addrinfo = *result;
  freeaddrinfo(result);
  return host_addrinfo;
}

template <bool NonBlocking = true> bool set_nonblocking(int sockfd) {
  int flags = fcntl(sockfd, F_GETFL, 0);
  if (flags == -1) {
    return false;
  }
  if (fcntl(sockfd, F_SETFL,
            NonBlocking ? (flags | O_NONBLOCK) : (flags & (~O_NONBLOCK))) ==
      -1) {
    return false;
  }
  return true;
}

http::Error wait_until_ready_socket(socket_t sockfd, Duration auto timeout) {
  struct pollfd pfd;
  std::memset(&pfd, 0, sizeof(pfd));
  pfd.fd = sockfd;
  pfd.events = POLLOUT;

  auto timeout_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(timeout).count();
  int ret = poll(&pfd, 1, timeout_ms);

  if (ret == 0) {
    return http::Error::ConnectionTimeout;
  }

  if (ret > 0 && pfd.revents & POLLOUT) {
    auto error = 0;
    socklen_t len = sizeof(error);
    auto res = getsockopt(sockfd, SOL_SOCKET, SO_ERROR,
                          reinterpret_cast<char *>(&error), &len);
    auto successful = res >= 0 && !error;
    return successful ? http::Error::Success : http::Error::Connection;
  }

  return http::Error::Connection;
}

bool set_sock_opt_time(socket_t sockfd, int level, int optname,
                       Duration auto timeout) {
  struct timeval tv;
  tv.tv_sec = std::chrono::duration_cast<std::chrono::seconds>(timeout).count();
  tv.tv_usec =
      std::chrono::duration_cast<std::chrono::microseconds>(timeout).count() %
      1'000'000;

  if (setsockopt(sockfd, level, optname, &tv, sizeof(tv)) < 0) {
    return false;
  }
  return true;
}

} // namespace

namespace http::detail {

void close_socket(int sockfd) {
  if (sockfd != INVALID_SOCKET) {
    close(sockfd);
  }
}

void shutdown_socket(int sockfd) {
  if (sockfd != INVALID_SOCKET) {
    shutdown(sockfd, SHUT_RDWR);
  }
}

socket_t create_client_socket(const std::string &host, uint16_t port,
                              std::chrono::microseconds connection_timeout,
                              std::chrono::microseconds read_timeout,
                              std::chrono::microseconds write_timeout,
                              Error &error) {
  auto host_addrinfo = get_host_addrinfo(host, port);
  if (!host_addrinfo) {
    error = Error::HostNotFound;
    return INVALID_SOCKET;
  }

  socket_t sockfd = socket(host_addrinfo->ai_family, host_addrinfo->ai_socktype,
                           host_addrinfo->ai_protocol);
  if (sockfd < 0) {
    error = Error::Connection;
    return INVALID_SOCKET;
  }
  auto guard = scope_guard::make_scope_exit([&]() { close_socket(sockfd); });

  // Temporarily set the socket to non-blocking mode to be able to use the
  // timeout during connect
  if (!set_nonblocking(sockfd)) {
    error = Error::Connection;
    return INVALID_SOCKET;
  }

  if (connect(sockfd, host_addrinfo->ai_addr, host_addrinfo->ai_addrlen) < 0) {
    if (errno != EINPROGRESS) {
      error = Error::Connection;
      return INVALID_SOCKET;
    }
  }

  error = wait_until_ready_socket(sockfd, connection_timeout);
  if (error != Error::Success) {
    return INVALID_SOCKET;
  }

  // Restore the socket to blocking mode
  if (!set_nonblocking<false>(sockfd)) {
    error = Error::Connection;
    return INVALID_SOCKET;
  }

  // Set the socket options for read and write timeouts
  if (!set_sock_opt_time(sockfd, SOL_SOCKET, SO_RCVTIMEO, read_timeout) ||
      !set_sock_opt_time(sockfd, SOL_SOCKET, SO_SNDTIMEO, write_timeout)) {
    error = Error::Connection;
    return INVALID_SOCKET;
  }

  guard.dismiss();
  error = Error::Success;
  return sockfd;
}

bool send_all(socket_t sockfd, std::span<const std::byte> data, Error &error) {
  while (data.size() > 0) {
    ssize_t ret = write(sockfd, data.data(), data.size());

    if (ret < 0) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        error = Error::WriteTimeout;
      } else {
        error = Error::Write;
      }

      return false;
    }

    data = data.subspan(ret);
  }

  error = Error::Success;
  return true;
}

ssize_t recv(socket_t sockfd, std::span<std::byte> data, size_t nbytes,
             Error &error) {
  ssize_t ret;
  do {
    ret = read(sockfd, data.data(), nbytes);
  } while (ret < 0 && errno == EINTR);

  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      error = Error::ReadTimeout;
    } else {
      error = Error::Read;
    }

    return -1;
  }

  error = Error::Success;
  return ret;
}

} // namespace http::detail
