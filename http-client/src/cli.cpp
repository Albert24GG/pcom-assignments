#include "cli.hpp"
#include "ctre.hpp"
#include "http/client.hpp"
#include "json.hpp"
#include "logger.hpp"
#include <format>
#include <functional>
#include <iostream>

namespace {
enum class Command {
  LOGIN_ADMIN = 0,
  ADD_USER,
  GET_USERS,
  DELETE_USER,
  LOGOUT_ADMIN,
  LOGIN_USER,
  GET_ACCESS,
  GET_MOVIES,
  GET_MOVIE,
  ADD_MOVIE,
  DELETE_MOVIE,
  UPDATE_MOVIE,
  GET_COLLECTIONS,
  GET_COLLECTION,
  ADD_COLLECTION,
  DELETE_COLLECTION,
  ADD_MOVIE_TO_COLLECTION,
  DELETE_MOVIE_FROM_COLLECTION,
  LOGOUT_USER,
  EXIT,
  TOTAL_COMMANDS,
  INVALID
};

Command from_str(std::string_view str) {
  const static auto command_map = std::unordered_map<std::string_view, Command>{
      {"login_admin", Command::LOGIN_ADMIN},
      {"add_user", Command::ADD_USER},
      {"get_users", Command::GET_USERS},
      {"delete_user", Command::DELETE_USER},
      {"logout_admin", Command::LOGOUT_ADMIN},
      {"login", Command::LOGIN_USER},
      {"get_access", Command::GET_ACCESS},
      {"get_movies", Command::GET_MOVIES},
      {"get_movie", Command::GET_MOVIE},
      {"add_movie", Command::ADD_MOVIE},
      {"delete_movie", Command::DELETE_MOVIE},
      {"update_movie", Command::UPDATE_MOVIE},
      {"get_collections", Command::GET_COLLECTIONS},
      {"get_collection", Command::GET_COLLECTION},
      {"add_collection", Command::ADD_COLLECTION},
      {"delete_collection", Command::DELETE_COLLECTION},
      {"add_movie_to_collection", Command::ADD_MOVIE_TO_COLLECTION},
      {"delete_movie_from_collection", Command::DELETE_MOVIE_FROM_COLLECTION},
      {"logout", Command::LOGOUT_USER},
      {"exit", Command::EXIT}};
  auto it = command_map.find(str);
  if (it != command_map.end()) {
    return it->second;
  }
  return Command::INVALID;
}

constexpr auto ARG_LINE_PATTERN = ctre::match<"^([[:graph:]]+)=(.+)$">;
constexpr auto SESSION_COOKIE_PATTERN = ctre::match<"session=[^;]*">;

/**
 * Read a line from standard input and parse it into a key-value pair.
 *
 * @param line_buffer The buffer to store the input line.
 * @return An optional pair of strings representing the key and value.
 */
[[nodiscard]] auto read_and_parse_arg_line(std::string &line_buffer)
    -> std::optional<std::pair<std::string, std::string>> {
  do {
    std::getline(std::cin, line_buffer);
  } while (line_buffer.empty());
  if (auto [whole, arg, value] = ARG_LINE_PATTERN.match(line_buffer); whole) {
    return {std::make_pair(arg.to_string(), value.to_string())};
  }
  return std::nullopt;
}

/**
 * Check if the HTTP response indicates success (2xx status code).
 *
 * @param response The HTTP response to check.
 * @return True if the response indicates success, false otherwise.
 */
bool is_success(const http::Response &response) {
  return response.status_code >= 200 && response.status_code < 300;
}

/**
 * Perform an HTTP request with retry logic.
 *
 * @param request_fn The function to perform the HTTP request.
 * @return An optional HTTP response.
 */
[[nodiscard]] http::Result
perform_http_request_with_retry(std::function<http::Result()> request_fn) {
  http::Result result;

  for (size_t i = 0; i < MAX_RETRY_COUNT; ++i) {
    result = request_fn();
    if (result) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return result;
}

void print_success(std::string_view message) {
  std::cout << "SUCCESS: " << message << "\n";
}

void print_error(std::string_view message) {
  std::cout << "ERROR: " << message << "\n";
}

} // namespace

void Cli::handle_result(
    const http::Result &result,
    std::function<void(const http::Response &)> on_response_ok,
    std::function<void(const http::Response &)> on_response_error,
    std::function<void(const http::Error)> on_request_failure) {
  if (result) {
    if (is_success(*result)) {
      on_response_ok(*result);
    } else {
      on_response_error(*result);
    }
  } else {
    on_request_failure(result.error());
  }
}

void Cli::handle_result(
    const http::Result &result,
    std::function<void(const http::Response &)> on_response_ok) {
  auto default_on_request_failure = [](const http::Error error) {
    print_error(to_str(error));
  };
  auto default_on_response_error = [](const http::Response &response) {
    print_error(
        std::format("{} - {}", response.status_code, response.status_message));
  };

  handle_result(result, on_response_ok, default_on_response_error,
                default_on_request_failure);
}

using json = nlohmann::json;

void Cli::handle_login_admin() {
  std::string username;
  if (auto args = read_and_parse_arg_line(line_buffer_);
      args && args->first == "username") {
    username = std::move(args->second);
  } else {
    print_error("Invalid username format. Expected 'username=<value>'");
    return;
  }

  std::string password;
  if (auto args = read_and_parse_arg_line(line_buffer_);
      args && args->first == "password") {
    password = std::move(args->second);
  } else {
    print_error("Invalid password format. Expected 'password=<value>'");
    return;
  }

  const static auto route = std::format("{}/admin/login", BASE_ROUTE);
  const json payload = {
      {"username", username},
      {"password", password},
  };

  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Post(route, payload.dump(), http_headers_); });
  handle_result(result, [this](const http::Response &response) {
    print_success("Admin logged in successfully");
    // Extract the session cookie from the response
    auto cookie_header = response.headers.find("Set-Cookie");

    if (cookie_header != response.headers.end()) {
      auto session_cookie =
          SESSION_COOKIE_PATTERN.search(cookie_header->second);
      if (session_cookie) {
        http_headers_.insert_or_assign("Cookie", std::move(session_cookie));
      }
    }
  });
}

void Cli::handle_add_user() {
  std::string username;
  if (auto args = read_and_parse_arg_line(line_buffer_);
      args && args->first == "username") {
    username = std::move(args->second);
  } else {
    print_error("Invalid username format. Expected 'username=<value>'");
    return;
  }

  std::string password;
  if (auto args = read_and_parse_arg_line(line_buffer_);
      args && args->first == "password") {
    password = std::move(args->second);
  } else {
    print_error("Invalid password format. Expected 'password=<value>'");
    return;
  }

  const static auto route = std::format("{}/admin/users", BASE_ROUTE);
  const json payload = {
      {"username", username},
      {"password", password},
  };

  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Post(route, payload.dump(), http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("User added successfully");
  });
}

void Cli::handle_get_users() {
  const static auto route = std::format("{}/admin/users", BASE_ROUTE);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    const json response_json = json::parse(response.body, nullptr, false);
    if (response_json.is_discarded()) {
      print_error("Failed to parse JSON response");
      return;
    }

    if (const auto users = response_json.find("users");
        users != response_json.end()) {
      std::ostringstream os;
      os << "Users retrieved successfully\n";
      bool is_success = true;
      for (size_t i = 0; i < users->size(); ++i) {
        const auto username = (*users)[i].find("username");
        const auto password = (*users)[i].find("password");
        if (username != (*users)[i].end() && password != (*users)[i].end()) {
          os << "#" << i + 1 << " " << username.value().get<std::string_view>()
             << ":" << password.value().get<std::string_view>();
          if (i + 1 != users->size()) {
            os << "\n";
          }
        } else {
          print_error("Invalid user data format");
          is_success = false;
          break;
        }
      }
      if (is_success) {
        print_success(os.str());
      }
    } else {
      print_error("'users' key not found in the response");
    }
  });
}

void Cli::handle_delete_user() {
  std::string username;
  if (auto args = read_and_parse_arg_line(line_buffer_);
      args && args->first == "username") {
    username = std::move(args->second);
  } else {
    print_error("Invalid username format. Expected 'username=<value>'");
    return;
  }

  const auto route = std::format("{}/admin/users/{}", BASE_ROUTE, username);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Delete(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("User deleted successfully");
  });
}

void Cli::handle_logout_admin() {
  const static auto route = std::format("{}/admin/logout", BASE_ROUTE);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [this](const http::Response &response) {
    print_success("Admin logged out successfully");
    // Remove the session cookie from the headers
    auto cookie_header = http_headers_.find("Cookie");
    if (cookie_header != http_headers_.end()) {
      auto session_cookie =
          SESSION_COOKIE_PATTERN.search(cookie_header->second);
      if (session_cookie) {
        http_headers_.erase(cookie_header);
      }
    }
  });
}

void Cli::handle_login_user() {
  std::string admin_username;
  if (auto args = read_and_parse_arg_line(line_buffer_);
      args && args->first == "admin_username") {
    admin_username = std::move(args->second);
  } else {
    print_error(
        "Invalid admin username format. Expected 'admin_username=<value>'");
    return;
  }

  std::string username;
  if (auto args = read_and_parse_arg_line(line_buffer_);
      args && args->first == "username") {
    username = std::move(args->second);
  } else {
    print_error("Invalid username format. Expected 'username=<value>'");
    return;
  }

  std::string password;
  if (auto args = read_and_parse_arg_line(line_buffer_);
      args && args->first == "password") {
    password = std::move(args->second);
  } else {
    print_error("Invalid password format. Expected 'password=<value>'");
    return;
  }

  const static auto route = std::format("{}/user/login", BASE_ROUTE);
  const json payload = {
      {"admin_username", admin_username},
      {"username", username},
      {"password", password},
  };
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Post(route, payload.dump(), http_headers_); });
  handle_result(result, [this](const http::Response &response) {
    print_success("User logged in successfully");
    // Extract the session cookie from the response
    auto cookie_header = response.headers.find("Set-Cookie");

    if (cookie_header != response.headers.end()) {
      auto session_cookie =
          SESSION_COOKIE_PATTERN.search(cookie_header->second);
      if (session_cookie) {
        http_headers_.insert_or_assign("Cookie", std::move(session_cookie));
      }
    }
  });
}

void Cli::handle_logout_user() {
  const static auto route = std::format("{}/user/logout", BASE_ROUTE);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [this](const http::Response &response) {
    print_success("User logged out successfully");
    // Remove the session cookie from the headers
    auto cookie_header = http_headers_.find("Cookie");
    if (cookie_header != http_headers_.end()) {
      auto session_cookie =
          SESSION_COOKIE_PATTERN.search(cookie_header->second);
      if (session_cookie) {
        http_headers_.erase(cookie_header);
      }
    }
    // Remove the JWT token from the headers
    auto jwt_token_header = http_headers_.find("Authorization");
    if (jwt_token_header != http_headers_.end()) {
      http_headers_.erase(jwt_token_header);
    }
  });
}

void Cli::handle_get_access() {
  const static auto route = std::format("{}/library/access", BASE_ROUTE);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [this](const http::Response &response) {
    const json response_json = json::parse(response.body, nullptr, false);
    if (response_json.is_discarded()) {
      print_error("Failed to parse JSON response");
      return;
    }

    if (const auto jwt_token = response_json.find("token");
        jwt_token != response_json.end()) {
      print_success("JWT token retrieved successfully");
      // Add the JWT token to the headers
      http_headers_.insert_or_assign(
          "Authorization",
          std::format("Bearer {}", jwt_token.value().get<std::string_view>()));
    } else {
      print_error("'token' key not found in the response");
    }
  });
}
void Cli::handle_exit() { should_exit_ = true; }

void Cli::run() {
  // Use application/json as the default content type
  http_headers_.try_emplace("Content-Type", "application/json");

  // Enable logging for the HTTP client
  auto log_fn = [](const http::Request &req, const http::Response &res) {
    std::ostringstream os;
    os << "Request:\n"
       << "METHOD: " << http::to_string(req.method) << "\nPATH: " << req.path
       << "\nPROTOCOL: " << req.protocol << "\nHEADERS:\n";
    for (const auto &[header, value] : req.headers) {
      os << header << " - " << value << "\n";
    }

    os << "\n\nResponse:\n"
       << res.version << " - " << res.status_code << " - " << res.status_message
       << "\nBODY: " << res.body << "\nHEADERS:\n";
    for (const auto &[header, value] : res.headers) {
      os << header << " - " << value << "\n";
    }
    LOG_INFO("{}", os.str());
  };
  http_client_.set_logger(log_fn);

  while (!should_exit_) {
    do {
      std::getline(std::cin, line_buffer_);
    } while (line_buffer_.empty());

    auto command = from_str(line_buffer_);
    switch (command) {
    case Command::LOGIN_ADMIN:
      handle_login_admin();
      break;
    case Command::EXIT:
      handle_exit();
      break;
    case Command::ADD_USER:
      handle_add_user();
      break;
    case Command::GET_USERS:
      handle_get_users();
      break;
    case Command::DELETE_USER:
      handle_delete_user();
      break;
    case Command::LOGOUT_ADMIN:
      handle_logout_admin();
      break;
    case Command::LOGIN_USER:
      handle_login_user();
      break;
    case Command::LOGOUT_USER:
      handle_logout_user();
      break;
    case Command::GET_ACCESS:
      handle_get_access();
      break;
    default:
      std::cerr << "Invalid command\n";
      break;
    }
  }

  std::cout << "Exiting...\n";
}
