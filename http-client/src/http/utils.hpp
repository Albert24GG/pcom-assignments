#pragma once

#include <chrono>
#include <type_traits>

namespace http::utils {

template <typename T>
concept Duration =
    requires {
      typename T::rep;
      typename T::period;
    } &&
    std::is_same_v<T,
                   std::chrono::duration<typename T::rep, typename T::period>>;

// Possible implementation of c++23 std::unreachable() taken from cppreference
// https://en.cppreference.com/w/cpp/utility/unreachable
[[noreturn]] inline void unreachable() {
#if defined(_MSC_VER) && !defined(__clang__) // MSVC
  __assume(false);
#else // GCC, Clang
  __builtin_unreachable();
#endif
}

} // namespace http::utils
