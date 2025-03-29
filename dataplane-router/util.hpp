#pragma once

#include <type_traits>

namespace router::util {

template <auto T, auto...> constexpr auto deferred_value = T;

template <typename T, typename = std::enable_if_t<std::is_integral_v<T> &&
                                                  std::is_unsigned_v<T>>>
T constexpr hton(T value) {

#if __BYTE_ORDER__ == __BIG_ENDIAN
  return value;
#else
  constexpr auto t_size = sizeof(T);
  if constexpr (t_size == 1) {
    return value;
  } else if constexpr (t_size == 2) {
    return __builtin_bswap16(value);
  } else if constexpr (t_size == 4) {
    return __builtin_bswap32(value);
  } else if constexpr (t_size == 8) {
    return __builtin_bswap64(value);
  } else {
    // Use a deferred false static_assert to avoid compilation errors
    static_assert(deferred_value<false>, "Unsupported type for hton");
  }
#endif
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T> &&
                                                  std::is_unsigned_v<T>>>
T constexpr ntoh(T value) {
  return hton(value);
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T> &&
                                                  std::is_unsigned_v<T>>>
constexpr int countl_zero(T value) {
  constexpr auto t_size = sizeof(T);

  if (value == 0) {
    return t_size * 8;
  }

  if constexpr (t_size == 1) {
    return __builtin_clz(value) - 24;
  } else if constexpr (t_size == 2) {
    return __builtin_clz(value) - 16;
  } else if constexpr (t_size == 4) {
    return __builtin_clz(value);
  } else if constexpr (t_size == 8) {
    return __builtin_clzll(value);
  } else {
    // Use a deferred false static_assert to avoid compilation errors
    static_assert(deferred_value<false>, "Unsupported type for countl_zero");
  }
}

template <typename T, typename = std::enable_if_t<std::is_integral_v<T> &&
                                                  std::is_unsigned_v<T>>>
constexpr int countl_one(T value) {
  return countl_zero(~value);
}

} // namespace router::util