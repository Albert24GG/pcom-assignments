#pragma once

#include "error.hpp"
#include "socket.hpp"
#include <chrono>
#include <cstddef>
#include <span>

namespace http::detail {

socket_t create_client_socket(const std::string &host, uint16_t port,
                              std::chrono::microseconds connection_timeout,
                              std::chrono::microseconds read_timeout,
                              std::chrono::microseconds write_timeout,
                              Error &error);

void close_socket(int sockfd);

void shutdown_socket(int sockfd);

bool send_all(socket_t sockfd, std::span<const std::byte> data, Error &error);

ssize_t recv(socket_t sockfd, std::span<std::byte> data, size_t nbytes,
             Error &error);

} // namespace http::detail
