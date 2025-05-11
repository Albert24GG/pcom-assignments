#pragma once

namespace http {

enum class Error {
  Success = 0,
  Unknown,
  HostNotFound,
  Connection,
  ConnectionTimeout,
  Read,
  ReadTimeout,
  Write,
  WriteTimeout
};

constexpr auto to_str(Error error) {
  switch (error) {
  case Error::Success:
    return "Success (no error)";
  case Error::Connection:
    return "Could not establish connection";
  case Error::ConnectionTimeout:
    return "Connection timed out";
  case Error::HostNotFound:
    return "Host not found";
  case Error::Read:
    return "Failed to read from socket";
  case Error::ReadTimeout:
    return "Socket read timed out";
  case Error::Write:
    return "Failed to write to socket";
  case Error::WriteTimeout:
    return "Socket write timed out";
  case Error::Unknown:
    return "Unknown error";
  default:
    return "Invalid error code";
  }
}

} // namespace http
