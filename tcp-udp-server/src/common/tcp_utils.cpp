#include "tcp_utils.hpp"

#include <cstring>
#include <sys/socket.h>

void send_all(int sockfd, const std::byte *buffer, size_t buffer_size) {
  size_t total_sent = 0;

  while (total_sent < buffer_size) {
    ssize_t sent = send(sockfd, buffer, buffer_size - total_sent, 0);

    if (sent < 0) {
      if (errno == EINTR) {
        // Interrupted by a signal, retry sending
        continue;
      } else if (errno == EPIPE || errno == ECONNRESET) {
        // Connection closed
        throw TcpConnectionClosed("Connection closed by peer");
      }
      // Other send error
      throw TcpTransmissionError("send() failed with error: " +
                                 std::string(std::strerror(errno)));
    }

    total_sent += static_cast<size_t>(sent);
    buffer += sent;
  }
}

void recv_all(int sockfd, std::byte *buffer, size_t buffer_size) {
  size_t total_received = 0;

  while (total_received < buffer_size) {
    ssize_t received = recv(sockfd, buffer, buffer_size - total_received, 0);

    if (received < 0) {
      if (errno == EINTR) {
        // Interrupted by a signal, retry receiving
        continue;
      }
      throw TcpTransmissionError("recv() failed with error: " +
                                 std::string(std::strerror(errno)));

    } else if (received == 0) {
      // Connection closed
      throw TcpConnectionClosed("Connection closed by peer");
    }

    total_received += static_cast<size_t>(received);
    buffer += received;
  }
}
