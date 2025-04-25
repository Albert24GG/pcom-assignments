#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

class TcpSocketException : public std::runtime_error {
public:
  explicit TcpSocketException(const std::string &message)
      : std::runtime_error(message) {}
};

class TcpConnectionClosed : public TcpSocketException {
public:
  explicit TcpConnectionClosed(const std::string &message)
      : TcpSocketException(message) {}
};

class TcpTransmissionError : public TcpSocketException {
public:
  explicit TcpTransmissionError(const std::string &message)
      : TcpSocketException(message) {}
};

/**
 * @brief Sends all bytes in the buffer to the socket.
 *
 * @param sockfd The socket file descriptor.
 * @param buffer The buffer to send.
 * @param buffer_size The size of the buffer.
 *
 * @throws TcpSocketException If the send fails.
 */
void send_all(int sockfd, const std::byte *buffer, size_t buffer_size);

/**
 * @brief Receives all bytes from the socket into the buffer.
 *
 * @param sockfd The socket file descriptor.
 * @param buffer The buffer to receive data into.
 * @param buffer_size The size of the buffer.
 *
 * @throws TcpSocketException If the receive fails.
 */
void recv_all(int sockfd, std::byte *buffer, size_t buffer_size);
