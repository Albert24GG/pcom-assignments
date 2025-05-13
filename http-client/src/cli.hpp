#pragma once

#include "http/client.hpp"

static constexpr std::string_view BASE_ROUTE = "/api/v1/tema";
static constexpr size_t MAX_RETRY_COUNT = 3;

class Cli {
public:
  Cli(std::string host, uint16_t port = 80)
      : http_client_(std::move(host), port) {}

  Cli() = delete;
  Cli(const Cli &) = delete;
  Cli &operator=(const Cli &) = delete;
  Cli(Cli &&) = default;
  Cli &operator=(Cli &&) = default;

  void run();

private:
  // Command handlers
  void handle_login_admin();
  void handle_add_user();
  void handle_get_users();
  void handle_delete_user();
  void handle_logout_admin();
  void handle_login_user();
  void handle_logout_user();
  void handle_get_access();
  void handle_get_movies();
  void handle_get_movie();
  void handle_add_movie();
  void handle_update_movie();
  void handle_delete_movie();
  void handle_exit();

  void
  handle_result(const http::Result &result,
                std::function<void(const http::Response &)> on_response_ok,
                std::function<void(const http::Response &)> on_response_error,
                std::function<void(const http::Error)> on_request_failure);
  void
  handle_result(const http::Result &result,
                std::function<void(const http::Response &)> on_response_ok);

  std::string line_buffer_;
  http::Client http_client_;
  http::Headers http_headers_;
  bool should_exit_ = false;
};
