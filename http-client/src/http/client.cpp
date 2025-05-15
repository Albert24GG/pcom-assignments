#include "client.hpp"

#include "constants.hpp"
#include "ctre.hpp"
#include "error.hpp"
#include "scope_guard.hpp"
#include "socket.hpp"
#include "socket_utils.hpp"
#include <iostream>
#include <optional>
#include <sstream>
#include <string_view>

namespace http::detail {
constexpr auto HEADER_LINE_MATCHER =
    ctre::match<"^([A-Za-z0-9\\-]+):\\s*(.+)$">;
constexpr auto STATUS_LINE_MATCHER =
    ctre::match<"^(HTTP\\/1\\.[01])\\s(\\d{3})(?:\\s(.*?))$">;
} // namespace http::detail

namespace http {

using namespace detail;

std::string Request::to_http_string() const {
  std::ostringstream os;

  os << to_string(method) << " " << path << " " << protocol << "\r\n";

  for (const auto &[header, value] : headers) {
    os << header << ": " << value << "\r\n";
  }

  if (!body.length() && !headers.contains("Content-Length")) {
    os << "Content-Length: " << body.length() << "\r\n";
  }

  os << "\r\n";

  if (!body.empty()) {
    os << body;
  }

  return os.str();
}

auto Response::from_str(std::string_view response_str)
    -> std::optional<Response> {

  auto header_end = response_str.find(constants::HTTP_HEADER_TERMINATOR);

  if (header_end == std::string_view::npos) {
    return std::nullopt;
  }

  Response res;

  {
    auto status_line_view = response_str.substr(0, response_str.find("\r\n"));
    if (auto [whole, version, status_code, status_message] =
            STATUS_LINE_MATCHER(status_line_view);
        whole) {
      if (auto status_code_num = status_code.to_optional_number();
          status_code_num) {
        res.status_code = *status_code_num;
      } else {
        // Invalid status code
        return std::nullopt;
      }
      res.version = version;
      res.status_message = status_message;
    } else {
      return std::nullopt;
    }
  }

  size_t cur_pos = response_str.find("\r\n") + 2;
  response_str = response_str.substr(cur_pos);

  // Parse the header lines
  while (cur_pos < header_end) {
    size_t line_end = response_str.find("\r\n");
    if (line_end == std::string_view::npos) {
      break;
    }

    auto line_view = response_str.substr(0, line_end);
    if (auto [whole, header, value] = HEADER_LINE_MATCHER(line_view); whole) {
      res.headers.insert_or_assign(header.to_string(), value);
    } else {
      // Invalid header line
      return std::nullopt;
    }

    response_str = response_str.substr(line_end + 2);
    cur_pos += line_end + 2;
  }

  // Parse the body
  res.body = response_str.substr(2);

  return res;
}

void Client::close_connection() {
  shutdown_socket(socket_.sockfd);
  close_socket(socket_.sockfd);
  socket_.sockfd = INVALID_SOCKET;
}

bool Client::create_and_connect_socket(Error &error) {
  socket_t socket = create_client_socket(host_, port_, connection_timeout_,
                                         read_timeout_, write_timeout_, error);

  if (socket == INVALID_SOCKET) {
    return false;
  }

  socket_.sockfd = socket;
  return true;
}

auto Client::receive_response_data(Error &error) -> std::optional<std::string> {
  std::array<std::byte, constants::READ_BUFFER_SIZE> buf;
  std::string response_str;

  size_t content_length{};
  bool header_complete = false;
  size_t header_length{};

  // Read the header
  while (!header_complete) {
    ssize_t bytes = recv(socket_.sockfd, std::span(buf), buf.size(), error);

    if (bytes < 0) {
      return std::nullopt;
    } else if (bytes == 0) {
      break;
    }

    auto received_data_view =
        std::string_view(reinterpret_cast<const char *>(buf.data()), bytes);
    response_str.append(received_data_view);
    {
      // Search the header terminator only in the last part of the response (if
      // possible)
      auto searchable_str_view = std::string_view(response_str);
      if (response_str.length() >=
          received_data_view.length() +
              constants::HTTP_HEADER_TERMINATOR.length()) {
        searchable_str_view = searchable_str_view.substr(
            response_str.length() - received_data_view.length() -
            constants::HTTP_HEADER_TERMINATOR.length());
      }

      const auto header_terminator_pos =
          searchable_str_view.find(constants::HTTP_HEADER_TERMINATOR);

      if (header_terminator_pos != std::string_view::npos) {
        header_complete = true;
        header_length = response_str.length() - searchable_str_view.length() +
                        header_terminator_pos +
                        constants::HTTP_HEADER_TERMINATOR.length();
      }
    }
  }

  if (!header_complete) {
    if (error == Error::Success) {
      error = Error::Read;
    }
    return std::nullopt;
  }

  // Check for Content-Length
  {
    auto response_str_view = std::string_view(response_str);
    constexpr auto content_length_finder =
        ctre::search<"content-length:\\s*(\\d+)", ctre::case_insensitive>;
    if (auto [whole, len] = content_length_finder(response_str_view); whole) {
      content_length = len.to_number();
    }
  }

  size_t response_length = header_length + content_length;

  while (response_str.length() < response_length) {
    ssize_t bytes = recv(socket_.sockfd, std::span(buf), buf.size(), error);

    if (bytes < 0) {
      return std::nullopt;
    } else if (bytes == 0) {
      break;
    }

    response_str.append(
        std::string_view(reinterpret_cast<const char *>(buf.data()), bytes));
  }

  if (response_str.length() < response_length) {
    if (error == Error::Success) {
      error = Error::Read;
    }
    return std::nullopt;
  }

  if (response_str.length() > response_length) {
    response_str.resize(content_length);
  }

  return response_str;
}

auto Client::process_request(Request request, Error &error)
    -> std::optional<Response> {

  auto guard = scope_guard::make_scope_exit([&] { close_connection(); });
  if (!socket_.is_open()) {
    if (!create_and_connect_socket(error)) {
      return std::nullopt;
    }
  }

  if (request.body.length() > 0) {
    request.headers.insert_or_assign("Content-Length",
                                     std::to_string(request.body.length()));
  }
  request.headers.insert_or_assign("Host", host_);

  std::string request_data = request.to_http_string();

  if (!send_all(socket_.sockfd, std::as_bytes(std::span(request_data)),
                error)) {
    return std::nullopt;
  }

  auto response_data_opt = receive_response_data(error);
  if (!response_data_opt) {
    return std::nullopt;
  }
  guard.dismiss();

  std::string response_str = std::move(*response_data_opt);

  auto response = Response::from_str(response_str);
  if (!response) {
    error = Error::Read;
    return std::nullopt;
  }

  // If Connection: close is set, close the connection
  {
    auto connection_header = response->headers.find("Connection");
    if (connection_header != response->headers.end() &&
        connection_header->second == "close") {
      close_connection();
    }
  }

  log(request, *response);
  return response;
}

Result Client::Get(const std::string &path) {
  Request request{
      .method = RequestMethod::GET,
      .path = path,
  };
  return Get(request);
}

Result Client::Get(const std::string &path, Headers headers) {
  Request request{.method = RequestMethod::GET,
                  .path = path,
                  .headers = std::move(headers)};
  return Get(request);
}

Result Client::Get(const Request &request) {
  Error error = Error::Success;
  auto response = process_request(request, error);

  return Result{std::move(response), error};
}

Result Client::Post(const std::string &path) {
  Request request{
      .method = RequestMethod::POST,
      .path = path,
      .body = "",
  };
  return Post(request);
}

Result Client::Post(const std::string &path, Headers headers) {
  Request request{.method = RequestMethod::POST,
                  .path = path,
                  .headers = std::move(headers),
                  .body = ""};
  return Post(request);
}

Result Client::Post(const std::string &path, const std::string &body) {
  Request request{.method = RequestMethod::POST, .path = path, .body = body};
  return Post(request);
}

Result Client::Post(const std::string &path, const std::string &body,
                    Headers headers) {
  Request request{.method = RequestMethod::POST,
                  .path = path,
                  .headers = std::move(headers),
                  .body = body};
  return Post(request);
}

Result Client::Post(const Request &request) {
  Error error = Error::Success;
  auto response = process_request(request, error);

  return Result{std::move(response), error};
}

Result Client::Put(const std::string &path) {
  Request request{
      .method = RequestMethod::PUT,
      .path = path,
      .body = "",
  };
  return Put(request);
}

Result Client::Put(const std::string &path, Headers headers) {
  Request request{.method = RequestMethod::PUT,
                  .path = path,
                  .headers = std::move(headers),
                  .body = ""};
  return Put(request);
}

Result Client::Put(const std::string &path, const std::string &body) {
  Request request{.method = RequestMethod::PUT, .path = path, .body = body};
  return Put(request);
}

Result Client::Put(const std::string &path, const std::string &body,
                   Headers headers) {
  Request request{.method = RequestMethod::PUT,
                  .path = path,
                  .headers = std::move(headers),
                  .body = body};
  return Put(request);
}

Result Client::Put(const Request &request) {
  Error error = Error::Success;
  auto response = process_request(request, error);

  return Result{std::move(response), error};
}

Result Client::Delete(const std::string &path) {
  Request request{
      .method = RequestMethod::DELETE,
      .path = path,
  };
  return Delete(request);
}

Result Client::Delete(const std::string &path, Headers headers) {
  Request request{.method = RequestMethod::DELETE,
                  .path = path,
                  .headers = std::move(headers)};
  return Delete(request);
}

Result Client::Delete(const Request &request) {
  Error error = Error::Success;
  auto response = process_request(request, error);

  return Result{std::move(response), error};
}

} // namespace http
