#pragma once

namespace http::detail {

static constexpr int INVALID_SOCKET{-1};

using socket_t = int;

struct Socket {

  socket_t sockfd{INVALID_SOCKET};

  bool is_open() const { return sockfd != INVALID_SOCKET; }
};

} // namespace http::detail
