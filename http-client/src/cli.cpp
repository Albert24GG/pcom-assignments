#include "cli.hpp"
#include "ctre.hpp"
#include "fmt/format.h"
#include "http/client.hpp"
#include "json.hpp"
#include "logger.hpp"
#include <functional>
#include <iostream>
#include <string_view>
#include <type_traits>
#include <thread>

namespace {
constexpr auto SESSION_COOKIE_FINDER = ctre::search<"session=[^;]*">;

/**
 * Read a line from standard input and parse it into a key-value pair.
 *
 * @param line_buffer The buffer to store the input line.
 * @param arg_name The name of the argument to read.
 * @param delimiter The delimiter used to separate the key and value.
 *
 * @return An optional pair of strings representing the key and value.
 *
 * @throws std::invalid_argument if the input format is invalid.
 */
template <typename ValueType = std::string>
[[nodiscard]] ValueType
read_and_parse_arg_line(std::string &line_buffer, std::string_view arg_name,
                        std::function<bool (const ValueType&)> validator = [](const ValueType&) { return true; }) {
  std::cout << arg_name << '=';
  std::flush(std::cout);

  do {
    std::getline(std::cin, line_buffer);
  } while (line_buffer.empty());

  if constexpr (std::is_arithmetic_v<ValueType>) {
    ValueType value{};
    if (auto [_, ec] = std::from_chars(
            line_buffer.data(), line_buffer.data() + line_buffer.size(), value);
        ec == std::errc() && validator(value)) {
      return value;
    } else {
      throw std::invalid_argument(
          fmt::format("Invalid value for field {}", arg_name));
    }
  } else if constexpr (std::is_constructible_v<ValueType, std::string>) {
    ValueType value(line_buffer);
    if(validator(value)) {
      return value;
    } else {
      throw std::invalid_argument(
          fmt::format("Invalid value for field {}", arg_name));
    }
  } else {
    static_assert(std::is_constructible_v<ValueType, std::string>,
                  "Unsupported type for read_and_parse_arg_line");
  }
}

bool has_no_spaces(const std::string &str) {
  return str.find(' ') == std::string::npos;
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

std::string dump_json_pretty(const nlohmann::json &json) {
  return json.dump(2);
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
    const auto response_json =
        nlohmann::json::parse(response.body, nullptr, false);
    if (!response.body.empty() && !response_json.is_discarded() &&
        response_json.contains("error")) {
      print_error(fmt::format("{}({}) - {}", response.status_code,
                              response.status_message,
                              response_json["error"].get<std::string_view>()));
    } else {
      print_error(
          fmt::format("{}({})", response.status_code, response.status_message));
    }
  };

  handle_result(result, on_response_ok, default_on_response_error,
                default_on_request_failure);
}

using json = nlohmann::json;

void Cli::handle_command(std::string_view command){
  const static auto str_to_cmd = std::unordered_map<std::string_view, void (Cli::*)()>{
      {"login_admin", &Cli::handle_login_admin},
      {"add_user", &Cli::handle_add_user},
      {"get_users", &Cli::handle_get_users},
      {"delete_user", &Cli::handle_delete_user},
      {"logout_admin", &Cli::handle_logout_admin},
      {"login", &Cli::handle_login_user},
      {"logout", &Cli::handle_logout_user},
      {"get_access", &Cli::handle_get_access},
      {"get_movies", &Cli::handle_get_movies},
      {"get_movie", &Cli::handle_get_movie},
      {"add_movie", &Cli::handle_add_movie},
      {"delete_movie", &Cli::handle_delete_movie},
      {"update_movie", &Cli::handle_update_movie},
      {"get_collections", &Cli::handle_get_collections},
      {"get_collection", &Cli::handle_get_collection},
      {"add_collection", &Cli::handle_add_collection},
      {"delete_collection", &Cli::handle_delete_collection},
      {"add_movie_to_collection",
       &Cli::handle_add_movie_to_collection},
      {"delete_movie_from_collection",
       &Cli::handle_delete_movie_from_collection},
      {"exit", &Cli::handle_exit},
  };

  if (auto it = str_to_cmd.find(command); it != str_to_cmd.end()) {
    std::invoke(it->second, this);
  } else {
    throw std::invalid_argument(
        fmt::format("Invalid command: {}", command));
  }
}

void Cli::handle_login_admin() {
  std::string username =
      read_and_parse_arg_line<std::string>(line_buffer_, "username",
                                          has_no_spaces);
  std::string password =
      read_and_parse_arg_line<std::string>(line_buffer_, "password", has_no_spaces);

  const static auto route = fmt::format("{}/admin/login", BASE_ROUTE);
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
      auto session_cookie = SESSION_COOKIE_FINDER(cookie_header->second);
      if (session_cookie) {
        http_headers_.insert_or_assign("Cookie", std::move(session_cookie));
      }
    }
  });
}

void Cli::handle_add_user() {
  std::string username =
      read_and_parse_arg_line<std::string>(line_buffer_, "username", has_no_spaces);
  std::string password =
      read_and_parse_arg_line<std::string>(line_buffer_, "password", has_no_spaces);

  const static auto route = fmt::format("{}/admin/users", BASE_ROUTE);
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
  const static auto route = fmt::format("{}/admin/users", BASE_ROUTE);
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
          os << "#" << i + 1 << " " << username->get<std::string_view>() << ":"
             << password->get<std::string_view>();
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
  std::string username =
      read_and_parse_arg_line<std::string>(line_buffer_, "username", has_no_spaces);

  const auto route = fmt::format("{}/admin/users/{}", BASE_ROUTE, username);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Delete(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("User deleted successfully");
  });
}

void Cli::handle_logout_admin() {
  const static auto route = fmt::format("{}/admin/logout", BASE_ROUTE);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [this](const http::Response &response) {
    print_success("Admin logged out successfully");
    // Remove the session cookie from the headers
    auto cookie_header = http_headers_.find("Cookie");
    if (cookie_header != http_headers_.end()) {
      auto session_cookie = SESSION_COOKIE_FINDER(cookie_header->second);
      if (session_cookie) {
        http_headers_.erase(cookie_header);
      }
    }
  });
}

void Cli::handle_login_user() {
  std::string admin_username =
      read_and_parse_arg_line<std::string>(line_buffer_, "admin_username",
                                          has_no_spaces);
  std::string username =
      read_and_parse_arg_line<std::string>(line_buffer_, "username", has_no_spaces);
  std::string password =
      read_and_parse_arg_line<std::string>(line_buffer_, "password", has_no_spaces);

  const static auto route = fmt::format("{}/user/login", BASE_ROUTE);
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
      auto session_cookie = SESSION_COOKIE_FINDER(cookie_header->second);
      if (session_cookie) {
        http_headers_.insert_or_assign("Cookie", std::move(session_cookie));
      }
    }
  });
}

void Cli::handle_logout_user() {
  const static auto route = fmt::format("{}/user/logout", BASE_ROUTE);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [this](const http::Response &response) {
    print_success("User logged out successfully");
    // Remove the session cookie from the headers
    auto cookie_header = http_headers_.find("Cookie");
    if (cookie_header != http_headers_.end()) {
      auto session_cookie = SESSION_COOKIE_FINDER(cookie_header->second);
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
  const static auto route = fmt::format("{}/library/access", BASE_ROUTE);
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
          fmt::format("Bearer {}", jwt_token->get<std::string_view>()));
    } else {
      print_error("'token' key not found in the response");
    }
  });
}

void Cli::handle_get_movies() {
  const static auto route = fmt::format("{}/library/movies", BASE_ROUTE);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    const json response_json = json::parse(response.body, nullptr, false);
    if (response_json.is_discarded()) {
      print_error("Failed to parse JSON response");
      return;
    }

    if (const auto movies = response_json.find("movies");
        movies != response_json.end()) {
      std::ostringstream os;
      os << "Movies retrieved successfully";
      bool is_success = true;
      for (size_t i = 0; i < movies->size(); ++i) {
        const auto title = (*movies)[i].find("title");
        const auto id = (*movies)[i].find("id");
        if (title != (*movies)[i].end() && id != (*movies)[i].end()) {
          os << "\n#" << *id << " " << title->get<std::string_view>();
        } else {
          print_error("Invalid movie data format");
          is_success = false;
          break;
        }
      }
      if (is_success) {
        print_success(os.str());
      }
    } else {
      print_error("'movies' key not found in the response");
    }
  });
}

void Cli::handle_get_movie() {
  size_t id = read_and_parse_arg_line<size_t>(line_buffer_, "id");

  const auto route = fmt::format("{}/library/movies/{}", BASE_ROUTE, id);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    const json response_json = json::parse(response.body, nullptr, false);
    if (response_json.is_discarded()) {
      print_error("Failed to parse JSON response");
      return;
    }
    print_success(fmt::format("Movie retrieved successfully\n{}",
                              dump_json_pretty(response_json)));
  });
}

void Cli::handle_add_movie() {
  std::string title =
      read_and_parse_arg_line<std::string>(line_buffer_, "title");
  size_t year = read_and_parse_arg_line<size_t>(line_buffer_, "year");
  std::string description =
      read_and_parse_arg_line<std::string>(line_buffer_, "description");
  double rating = read_and_parse_arg_line<double>(line_buffer_, "rating", [](const double &value) {
    return value >= 0.0 && value <= 10.0;
  });

  const static auto route = fmt::format("{}/library/movies", BASE_ROUTE);
  const json payload = {
      {"title", title},
      {"year", year},
      {"description", description},
      {"rating", rating},
  };
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Post(route, payload.dump(), http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("Movie added successfully");
  });
}

void Cli::handle_update_movie() {
  size_t id = read_and_parse_arg_line<size_t>(line_buffer_, "id");
  std::string title =
      read_and_parse_arg_line<std::string>(line_buffer_, "title");
  size_t year = read_and_parse_arg_line<size_t>(line_buffer_, "year");
  std::string description =
      read_and_parse_arg_line<std::string>(line_buffer_, "description");
  double rating = read_and_parse_arg_line<double>(line_buffer_, "rating", [](const double &value) {
    return value >= 0.0 && value <= 10.0;
  });

  const auto route = fmt::format("{}/library/movies/{}", BASE_ROUTE, id);
  const json payload = {
      {"title", title},
      {"year", year},
      {"description", description},
      {"rating", rating},
  };
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Put(route, payload.dump(), http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("Movie updated successfully");
  });
}

void Cli::handle_delete_movie() {
  size_t id = read_and_parse_arg_line<size_t>(line_buffer_, "id");

  const auto route = fmt::format("{}/library/movies/{}", BASE_ROUTE, id);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Delete(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("Movie deleted successfully");
  });
}

void Cli::handle_get_collections() {
  const static auto route = fmt::format("{}/library/collections", BASE_ROUTE);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    const json response_json = json::parse(response.body, nullptr, false);
    if (response_json.is_discarded()) {
      print_error("Failed to parse JSON response");
      return;
    }

    if (const auto collections = response_json.find("collections");
        collections != response_json.end()) {
      std::ostringstream os;
      os << "Collections retrieved successfully";
      bool is_success = true;
      for (size_t i = 0; i < collections->size(); ++i) {
        const auto title = (*collections)[i].find("title");
        const auto id = (*collections)[i].find("id");
        if (title != (*collections)[i].end() && id != (*collections)[i].end()) {
          os << "\n#" << *id << " " << title->get<std::string_view>();
          if (i + 1 != collections->size()) {
            os << "\n";
          }
        } else {
          print_error("Invalid collection data format");
          is_success = false;
          break;
        }
      }
      if (is_success) {
        print_success(os.str());
      }
    } else {
      print_error("'collections' key not found in the response");
    }
  });
}

void Cli::handle_get_collection() {
  size_t id = read_and_parse_arg_line<size_t>(line_buffer_, "id");

  const auto route = fmt::format("{}/library/collections/{}", BASE_ROUTE, id);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    const json response_json = json::parse(response.body, nullptr, false);
    if (response_json.is_discarded()) {
      print_error("Failed to parse JSON response");
      return;
    }
    std::ostringstream os;
    os << "Collection retrieved successfully\n";

    const auto title = response_json.find("title");
    const auto owner = response_json.find("owner");
    const auto movies = response_json.find("movies");
    bool is_success = true;

    if (title != response_json.end() && owner != response_json.end() &&
        movies != response_json.end()) {
      os << "title: " << title->get<std::string_view>() << "\n"
         << "owner: " << owner->get<std::string_view>() << "\n";

      for (size_t i = 0; i < movies->size(); ++i) {
        const auto title = (*movies)[i].find("title");
        const auto id = (*movies)[i].find("id");

        if (title != (*movies)[i].end() && id != (*movies)[i].end()) {
          os << "\n#" << *id << ": " << title->get<std::string_view>();
        } else {
          print_error("Invalid movie data format");
          is_success = false;
          break;
        }
      }

      if (is_success) {
        print_success(os.str());
      }
    } else {
      print_error("Invalid collection data format");
      is_success = false;
    }
  });
}

void Cli::handle_add_collection() {
  std::string title =
      read_and_parse_arg_line<std::string>(line_buffer_, "title");
  size_t num_movies =
      read_and_parse_arg_line<size_t>(line_buffer_, "num_movies");

  std::vector<size_t> movie_ids;
  for (size_t i = 0; i < num_movies; ++i) {
    size_t movie_id = read_and_parse_arg_line<size_t>(
        line_buffer_, fmt::format("movie_id[{}]", i));
    movie_ids.push_back(movie_id);
  }

  const static auto route = fmt::format("{}/library/collections", BASE_ROUTE);
  const json payload = {
      {"title", title},
  };

  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Post(route, payload.dump(), http_headers_); });
  handle_result(result, [this, &movie_ids](const http::Response &response) {
    const json response_json = json::parse(response.body, nullptr, false);
    if (response_json.is_discarded()) {
      print_error("Failed to parse JSON response");
      return;
    }

    size_t collection_id;
    if (const auto id = response_json.find("id"); id != response_json.end()) {
      collection_id = *id;
    } else {
      print_error("'id' key not found in the response");
      return;
    }

    const auto route = fmt::format("{}/library/collections/{}/movies",
                                   BASE_ROUTE, collection_id);
    size_t added_movies = 0;
    // Add each movie to the collection
    for (const auto &movie_id : movie_ids) {
      const json payload = {
          {"id", movie_id},
      };
      const auto result = perform_http_request_with_retry([&] {
        return http_client_.Post(route, payload.dump(), http_headers_);
      });
      handle_result(
          result,
          [&added_movies](const http::Response &response) { ++added_movies; },
          [](const http::Response &) {}, [](const http::Error) {});
    }

    if (added_movies == movie_ids.size()) {
      print_success("Collection added successfully");
    } else {
      print_error(
          fmt::format("Failed to add {} out of {} movies to the collection",
                      movie_ids.size() - added_movies, movie_ids.size()));
    }
  });
}

void Cli::handle_delete_collection() {
  size_t id = read_and_parse_arg_line<size_t>(line_buffer_, "id");

  const auto route = fmt::format("{}/library/collections/{}", BASE_ROUTE, id);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Delete(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("Collection deleted successfully");
  });
}

void Cli::handle_add_movie_to_collection() {
  size_t collection_id =
      read_and_parse_arg_line<size_t>(line_buffer_, "collection_id");
  size_t movie_id = read_and_parse_arg_line<size_t>(line_buffer_, "movie_id");

  const auto route = fmt::format("{}/library/collections/{}/movies", BASE_ROUTE,
                                 collection_id);
  const json payload = {
      {"id", movie_id},
  };
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Post(route, payload.dump(), http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("Movie added to collection successfully");
  });
}

void Cli::handle_delete_movie_from_collection() {
  size_t collection_id =
      read_and_parse_arg_line<size_t>(line_buffer_, "collection_id");
  size_t movie_id = read_and_parse_arg_line<size_t>(line_buffer_, "movie_id");

  const auto route = fmt::format("{}/library/collections/{}/movies/{}",
                                 BASE_ROUTE, collection_id, movie_id);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Delete(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("Movie deleted from collection successfully");
  });
}

void Cli::handle_exit() { should_exit_ = true; }

void Cli::run() {
  // Use application/json as the default content type
  http_headers_.try_emplace("Content-Type", "application/json");
  http_headers_.try_emplace("Accept", "application/json");

  // Enable logging for the HTTP client
  auto log_fn = [](const http::Request &req, const http::Response &res) {
    std::ostringstream os;
    os << "Request:\n"
       << "METHOD: " << http::to_string(req.method) << "\nPATH: " << req.path
       << "\nPROTOCOL: " << req.protocol << "\nHEADERS:\n";
    for (const auto &[header, value] : req.headers) {
      os << header << " - " << value << "\n";
    }
    os << "BODY: " << req.body << "\n";

    os << "\n\nResponse:\n"
       << res.version << " - " << res.status_code << " - " << res.status_message
       << "\nHEADERS:\n";
    for (const auto &[header, value] : res.headers) {
      os << header << " - " << value << "\n";
    }
    os << "BODY: " << res.body << "\n";
    LOG_INFO("{}", os.str());
  };
  http_client_.set_logger(log_fn);

  while (!should_exit_) {
    do {
      std::getline(std::cin, line_buffer_);
    } while (line_buffer_.empty());

    try {
      handle_command(line_buffer_);
    } catch (const std::exception &e) {
      print_error(e.what());
    }
  }

  std::cout << "Exiting...\n";
}
