#include "cli.hpp"
#include "ctre.hpp"
#include "http/client.hpp"
#include "json.hpp"
#include "logger.hpp"
#include <format>
#include <functional>
#include <iostream>
#include <type_traits>

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

constexpr auto SESSION_COOKIE_PATTERN = ctre::match<"session=[^;]*">;

/**
 * Read a line from standard input and parse it into a key-value pair.
 *
 * @param line_buffer The buffer to store the input line.
 * @param arg_name The name of the argument to read.
 * @param delimiter The delimiter used to separate the key and value.
 * @return An optional pair of strings representing the key and value.
 */
template <typename ValueType = std::string>
[[nodiscard]] auto read_and_parse_arg_line(std::string &line_buffer,
                                           std::string_view arg_name,
                                           std::string_view delimiter = "=")
    -> std::optional<ValueType> {
  std::cout << arg_name << delimiter;
  std::flush(std::cout);

  do {
    std::getline(std::cin, line_buffer);
  } while (line_buffer.empty());

  if constexpr (std::is_arithmetic_v<ValueType>) {
    ValueType value{};
    if (auto [_, ec] = std::from_chars(
            line_buffer.data(), line_buffer.data() + line_buffer.size(), value);
        ec == std::errc()) {
      return value;
    } else {
      return std::nullopt;
    }
  } else if constexpr (std::is_constructible_v<ValueType, std::string>) {
    return ValueType(line_buffer);
  } else {
    static_assert(std::is_constructible_v<ValueType, std::string>,
                  "Unsupported type for read_and_parse_arg_line");
  }
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
    if (!response.body.empty() && !response_json.is_discarded()) {
      print_error(std::format("{} - {}\n{}", response.status_code,
                              response.status_message,
                              dump_json_pretty(response_json)));
    } else {
      print_error(std::format("{} - {}", response.status_code,
                              response.status_message));
    }
  };

  handle_result(result, on_response_ok, default_on_response_error,
                default_on_request_failure);
}

using json = nlohmann::json;

void Cli::handle_login_admin() {
  std::string username;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "username");
      arg_value) {
    username = std::move(*arg_value);
  } else {
    print_error("Invalid username format. Expected 'username=<value>'");
    return;
  }

  std::string password;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "password");
      arg_value) {
    password = std::move(*arg_value);
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
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "username");
      arg_value) {
    username = std::move(*arg_value);
  } else {
    print_error("Invalid username format. Expected 'username=<value>'");
    return;
  }

  std::string password;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "password");
      arg_value) {
    password = std::move(*arg_value);
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
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "username");
      arg_value) {
    username = std::move(*arg_value);
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
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "admin_username");
      arg_value) {
    admin_username = std::move(*arg_value);
  } else {
    print_error(
        "Invalid admin username format. Expected 'admin_username=<value>'");
    return;
  }

  std::string username;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "username");
      arg_value) {
    username = std::move(*arg_value);
  } else {
    print_error("Invalid username format. Expected 'username=<value>'");
    return;
  }

  std::string password;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "password");
      arg_value) {
    password = std::move(*arg_value);
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

void Cli::handle_get_movies() {
  const static auto route = std::format("{}/library/movies", BASE_ROUTE);
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
          os << "\n#" << *id << " " << *title;
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
  size_t id;
  if (auto arg_value = read_and_parse_arg_line<size_t>(line_buffer_, "id");
      arg_value) {
    id = *arg_value;
  } else {
    print_error("Invalid movie ID format. Expected 'id=<value>'");
    return;
  }

  const auto route = std::format("{}/library/movies/{}", BASE_ROUTE, id);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Get(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    const json response_json = json::parse(response.body, nullptr, false);
    if (response_json.is_discarded()) {
      print_error("Failed to parse JSON response");
      return;
    }
    print_success(std::format("Movie retrieved successfully\n{}",
                              dump_json_pretty(response_json)));
  });
}

void Cli::handle_add_movie() {
  std::string title;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "title");
      arg_value) {
    title = std::move(*arg_value);
  } else {
    print_error("Invalid title format. Expected 'title=<value>'");
    return;
  }

  size_t year;
  if (auto arg_value = read_and_parse_arg_line<size_t>(line_buffer_, "year");
      arg_value) {
    year = *arg_value;
  } else {
    print_error("Invalid year format. Expected 'year=<value>'");
    return;
  }

  std::string description;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "description");
      arg_value) {
    description = std::move(*arg_value);
  } else {
    print_error("Invalid description format. Expected 'description=<value>'");
    return;
  }

  double rating;
  if (auto arg_value = read_and_parse_arg_line<double>(line_buffer_, "rating");
      arg_value) {
    rating = *arg_value;
  } else {
    print_error("Invalid rating format. Expected 'rating=<value>'");
    return;
  }

  const static auto route = std::format("{}/library/movies", BASE_ROUTE);
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
  size_t id;
  if (auto arg_value = read_and_parse_arg_line<size_t>(line_buffer_, "id");
      arg_value) {
    id = *arg_value;
  } else {
    print_error("Invalid movie ID format. Expected 'id=<value>'");
    return;
  }

  std::string title;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "title");
      arg_value) {
    title = std::move(*arg_value);
  } else {
    print_error("Invalid title format. Expected 'title=<value>'");
    return;
  }

  size_t year;
  if (auto arg_value = read_and_parse_arg_line<size_t>(line_buffer_, "year");
      arg_value) {
    year = *arg_value;
  } else {
    print_error("Invalid year format. Expected 'year=<value>'");
    return;
  }

  std::string description;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "description");
      arg_value) {
    description = std::move(*arg_value);
  } else {
    print_error("Invalid description format. Expected 'description=<value>'");
    return;
  }

  double rating;
  if (auto arg_value = read_and_parse_arg_line<double>(line_buffer_, "rating");
      arg_value) {
    rating = *arg_value;
  } else {
    print_error("Invalid rating format. Expected 'rating=<value>'");
    return;
  }

  const auto route = std::format("{}/library/movies/{}", BASE_ROUTE, id);
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
  size_t id;
  if (auto arg_value = read_and_parse_arg_line<size_t>(line_buffer_, "id");
      arg_value) {
    id = *arg_value;
  } else {
    print_error("Invalid movie ID format. Expected 'id=<value>'");
    return;
  }

  const auto route = std::format("{}/library/movies/{}", BASE_ROUTE, id);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Delete(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("Movie deleted successfully");
  });
}

void Cli::handle_get_collections() {
  const static auto route = std::format("{}/library/collections", BASE_ROUTE);
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
      os << "Collections retrieved successfully\n";
      bool is_success = true;
      for (size_t i = 0; i < collections->size(); ++i) {
        const auto title = (*collections)[i].find("title");
        const auto id = (*collections)[i].find("id");
        if (title != (*collections)[i].end() && id != (*collections)[i].end()) {
          os << "#" << *id << " " << *title;
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
  size_t id;
  if (auto arg_value = read_and_parse_arg_line<size_t>(line_buffer_, "id");
      arg_value) {
    id = *arg_value;
  } else {
    print_error("Invalid collection ID format. Expected 'id=<value>'");
    return;
  }

  const auto route = std::format("{}/library/collections/{}", BASE_ROUTE, id);
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
      os << "title: " << *title << "\n" << "owner: " << *owner;

      for (size_t i = 0; i < movies->size(); ++i) {
        const auto title = (*movies)[i].find("title");
        const auto id = (*movies)[i].find("id");

        if (title != (*movies)[i].end() && id != (*movies)[i].end()) {
          os << "\n#" << *id << ": " << *title;
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
  std::string title;
  if (auto arg_value = read_and_parse_arg_line(line_buffer_, "title");
      arg_value) {
    title = std::move(*arg_value);
  } else {
    print_error("Invalid title format. Expected 'title=<value>'");
    return;
  }

  size_t num_movies;
  if (auto arg_value =
          read_and_parse_arg_line<size_t>(line_buffer_, "num_movies");
      arg_value) {
    num_movies = *arg_value;
  } else {
    print_error(
        "Invalid number of movies format. Expected 'num_movies=<value>'");
    return;
  }

  std::vector<size_t> movie_ids;
  for (size_t i = 0; i < num_movies; ++i) {
    size_t movie_id;
    if (auto arg_value = read_and_parse_arg_line<size_t>(
            line_buffer_, std::format("movie_id[{}]", i));
        arg_value) {
      movie_id = *arg_value;
      movie_ids.push_back(movie_id);
    } else {
      print_error("Invalid movie ID format. Expected 'movie_id=<value>'");
      return;
    }
  }

  const static auto route = std::format("{}/library/collections", BASE_ROUTE);
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

    const auto route = std::format("{}/library/collections/{}/movies",
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
          std::format("Failed to add {} out of {} movies to the collection",
                      movie_ids.size() - added_movies, movie_ids.size()));
    }
  });
}

void Cli::handle_delete_collection() {
  size_t id;
  if (auto arg_value = read_and_parse_arg_line<size_t>(line_buffer_, "id");
      arg_value) {
    id = *arg_value;
  } else {
    print_error("Invalid collection ID format. Expected 'id=<value>'");
    return;
  }

  const auto route = std::format("{}/library/collections/{}", BASE_ROUTE, id);
  const auto result = perform_http_request_with_retry(
      [&] { return http_client_.Delete(route, http_headers_); });
  handle_result(result, [](const http::Response &response) {
    print_success("Collection deleted successfully");
  });
}

void Cli::handle_add_movie_to_collection() {
  size_t collection_id;
  if (auto arg_value =
          read_and_parse_arg_line<size_t>(line_buffer_, "collection_id");
      arg_value) {
    collection_id = *arg_value;
  } else {
    print_error(
        "Invalid collection ID format. Expected 'collection_id=<value>'");
    return;
  }

  size_t movie_id;
  if (auto arg_value =
          read_and_parse_arg_line<size_t>(line_buffer_, "movie_id");
      arg_value) {
    movie_id = *arg_value;
  } else {
    print_error("Invalid movie ID format. Expected 'movie_id=<value>'");
    return;
  }

  const auto route = std::format("{}/library/collections/{}/movies", BASE_ROUTE,
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
  size_t collection_id;
  if (auto arg_value =
          read_and_parse_arg_line<size_t>(line_buffer_, "collection_id");
      arg_value) {
    collection_id = *arg_value;
  } else {
    print_error(
        "Invalid collection ID format. Expected 'collection_id=<value>'");
    return;
  }

  size_t movie_id;
  if (auto arg_value =
          read_and_parse_arg_line<size_t>(line_buffer_, "movie_id");
      arg_value) {
    movie_id = *arg_value;
  } else {
    print_error("Invalid movie ID format. Expected 'movie_id=<value>'");
    return;
  }

  const auto route = std::format("{}/library/collections/{}/movies/{}",
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
    case Command::GET_MOVIES:
      handle_get_movies();
      break;
    case Command::GET_MOVIE:
      handle_get_movie();
      break;
    case Command::ADD_MOVIE:
      handle_add_movie();
      break;
    case Command::UPDATE_MOVIE:
      handle_update_movie();
      break;
    case Command::DELETE_MOVIE:
      handle_delete_movie();
      break;
    case Command::GET_COLLECTIONS:
      handle_get_collections();
      break;
    case Command::GET_COLLECTION:
      handle_get_collection();
      break;
    case Command::ADD_COLLECTION:
      handle_add_collection();
      break;
    case Command::DELETE_COLLECTION:
      handle_delete_collection();
      break;
    case Command::ADD_MOVIE_TO_COLLECTION:
      handle_add_movie_to_collection();
      break;
    case Command::DELETE_MOVIE_FROM_COLLECTION:
      handle_delete_movie_from_collection();
      break;
    default:
      std::cerr << "Invalid command\n";
      break;
    }
  }

  std::cout << "Exiting...\n";
}
