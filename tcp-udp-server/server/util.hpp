#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>

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

// The hash_combine function taken from boost
// https://www.boost.org/doc/libs/1_81_0/boost/container_hash/hash.hpp
// https://www.boost.org/doc/libs/1_81_0/boost/container_hash/detail/hash_mix.hpp

template <std::size_t WORD_SIZE> struct hash_mix_impl;

template <> struct hash_mix_impl<4> {
  static constexpr uint32_t fn(uint32_t x) {
    uint32_t const m1 = 0x21f0aaad;
    uint32_t const m2 = 0x735a2d97;

    x ^= x >> 16;
    x *= m1;
    x ^= x >> 15;
    x *= m2;
    x ^= x >> 15;

    return x;
  }
};

template <> struct hash_mix_impl<8> {
  static constexpr uint64_t fn(uint64_t x) {
    uint64_t const m = (uint64_t(0xe9846af) << 32) + 0x9b1a615d;

    x ^= x >> 32;
    x *= m;
    x ^= x >> 32;
    x *= m;
    x ^= x >> 28;

    return x;
  }
};

constexpr std::size_t hash_mix(std::size_t v) {
  return hash_mix_impl<sizeof(std::size_t)>::fn(v);
}

template <typename T,
          typename = std::enable_if_t<std::is_invocable_v<std::hash<T>, T>>>
inline void hash_combine(std::size_t &seed, const T &v) {
  seed = hash_mix(seed + 0x9e3779b9 + std::hash<T>()(v));
}
