#pragma once

#include "ctre.hpp"
#include <chrono>
#include <string_view>

namespace http::constants {

using namespace std::string_view_literals;

constexpr auto DEFAULT_CONNECTION_TIMEOUT{std::chrono::seconds(10)};
constexpr auto DEFAULT_CLIENT_READ_TIMEOUT{std::chrono::seconds(10)};
constexpr auto DEFAULT_CLIENT_WRITE_TIMEOUT{std::chrono::seconds(5)};

constexpr size_t READ_BUFFER_SIZE{2048};

constexpr auto HTTP_HEADER_TERMINATOR = "\r\n\r\n"sv;

constexpr auto HEADER_LINE_PAT = ctre::match<"^([A-Za-z0-9\\-]+):\\s*(.+)$">;
constexpr auto STATUS_LINE_PAT =
    ctre::match<"^(HTTP\\/1\\.[01])\\s(\\d{3})(?:\\s(.*?))$">;

} // namespace http::constants
