#pragma once

#include "constants.hpp"
#include "error.hpp"
#include "socket.hpp"
#include "utils.hpp"
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>

namespace http {

using Headers = std::unordered_map<std::string, std::string>;

struct Response {
  static auto from_str(std::string_view response_str)
      -> std::optional<Response>;

  std::string version;
  int status_code = -1;
  std::string status_message;
  std::string body;
  Headers headers{};
};

enum class RequestMethod { UNDEFINED = 0, GET, HEAD, POST, PUT, DELETE };

constexpr auto to_string(RequestMethod req_method) {
  switch (req_method) {
  case http::RequestMethod::UNDEFINED:
    return "UNDEFINED";
  case RequestMethod::GET:
    return "GET";
  case RequestMethod::HEAD:
    return "HEAD";
  case RequestMethod::POST:
    return "POST";
  case RequestMethod::PUT:
    return "PUT";
  case RequestMethod::DELETE:
    return "DELETE";
  default:
    utils::unreachable();
  }
}

struct Request {
  void add_header(const std::string &key, const std::string &value) {
    headers.insert_or_assign(key, value);
  }

  std::string to_http_string() const;

  RequestMethod method;
  std::string path;
  static constexpr std::string_view protocol{"HTTP/1.1"};
  Headers headers{};
  std::string body;
};

class Result {
public:
  Result(std::optional<Response> response, Error error)
      : response_(std::move(response)), error_(error) {}

  operator bool() const { return response_.has_value(); }
  const Response &operator*() const { return *response_; }
  Response &operator*() { return *response_; }
  const Response *operator->() const { return &*response_; }
  Response *operator->() { return &*response_; }

  Error error() const { return error_; }

private:
  std::optional<Response> response_;
  Error error_ = Error::Unknown;
};

using Logger = std::function<void(const Request &, const Response &)>;

class Client {
public:
  Client(std::string host, uint16_t port = 80)
      : host_(std::move(host)), port_(port) {}

  void set_connection_timeout(utils::Duration auto timeout) {
    connection_timeout_ =
        std::chrono::duration_cast<std::chrono::microseconds>(timeout);
  }
  void set_read_timeout(utils::Duration auto timeout) {
    read_timeout_ =
        std::chrono::duration_cast<std::chrono::microseconds>(timeout);
    ;
  }
  void set_write_timeout(utils::Duration auto timeout) {
    write_timeout_ =
        std::chrono::duration_cast<std::chrono::microseconds>(timeout);
  }

  void set_logger(Logger logger) { logger_ = std::move(logger); }

  Result Get(const std::string &path);
  Result Get(const std::string &path, Headers headers);
  Result Get(const Request &);

  Result Post(const std::string &path);
  Result Post(const std::string &path, const std::string &body);
  Result Post(const std::string &path, Headers headers);
  Result Post(const std::string &path, const std::string &body,
              Headers headers);
  Result Post(const Request &);

  Result Put(const std::string &path);
  Result Put(const std::string &path, const std::string &body);
  Result Put(const std::string &path, Headers headers);
  Result Put(const std::string &path, const std::string &body, Headers headers);
  Result Put(const Request &);

  Result Delete(const std::string &path);
  Result Delete(const std::string &path, Headers headers);
  Result Delete(const Request &);

private:
  void close_connection();
  bool create_and_connect_socket(Error &error);
  void log(const Request &request, const Response &response) {
    if (logger_) {
      logger_(request, response);
    }
  }
  auto process_request(Request request, Error &error)
      -> std::optional<Response>;
  auto receive_response_data(Error &error) -> std::optional<std::string>;

  Logger logger_{};

  std::string host_;
  uint16_t port_;
  detail::Socket socket_{};

  std::chrono::microseconds connection_timeout_{
      constants::DEFAULT_CONNECTION_TIMEOUT};
  std::chrono::microseconds read_timeout_{
      constants::DEFAULT_CLIENT_READ_TIMEOUT};
  std::chrono::microseconds write_timeout_{
      constants::DEFAULT_CLIENT_WRITE_TIMEOUT};
};

} // namespace http
