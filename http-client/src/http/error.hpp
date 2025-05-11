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

}
