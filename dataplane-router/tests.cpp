#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "util.hpp"

#define FWD(...) static_cast<decltype(__VA_ARGS__) &&>(__VA_ARGS__)
#define LIFT(...)                                                              \
  [](auto &&...args) noexcept(noexcept(                                        \
      __VA_ARGS__(FWD(args)...))) -> decltype(__VA_ARGS__(FWD(args)...)) {     \
    return __VA_ARGS__(FWD(args)...);                                          \
  }

template <typename Fn, typename... Args>
constexpr bool invocable(Fn &&fn, Args &&...args) noexcept {
  return std::is_invocable_v<Fn, Args...>;
}

namespace endianness {
using namespace routing::util;

TEST_CASE("Host to Network Order::uint16_t") {
  uint16_t value = 0x1234;
  uint16_t expected = 0x3412;

  uint16_t result = host_to_network_order(value);
  CHECK(result == expected);
}

TEST_CASE("Host to Network Order::uint32_t") {
  uint32_t value = 0x12345678;
  uint32_t expected = 0x78563412;

  uint32_t result = host_to_network_order(value);
  CHECK(result == expected);
}

TEST_CASE("Host to Network Order::uint64_t") {
  uint64_t value = 0x123456789ABCDEF0;
  uint64_t expected = 0xF0DEBC9A78563412;

  uint64_t result = host_to_network_order(value);
  CHECK(result == expected);
}

TEST_CASE("Host to Network Order::invalid type") {
  static_assert(!invocable(LIFT(host_to_network_order), 0.0f),
                "host_to_network_order should not be invocable with float");
  static_assert(!invocable(LIFT(host_to_network_order), 0.0),
                "host_to_network_order should not be invocable with double");
}

} // namespace endianness

namespace serialization {
using namespace routing::util;

struct POD {
  uint16_t a;
  uint32_t b;
  uint64_t c;

  constexpr auto to_tuple() { return std::tie(a, b, c); }
};

struct PODWithArray {
  uint16_t a;
  uint32_t b;
  std::array<uint8_t, 4> c;

  constexpr auto to_tuple() { return std::tie(a, b, c); }
};

struct NonPOD {
  uint16_t a;
  uint32_t b;
  std::string c;

  constexpr auto to_tuple() { return std::tie(a, b, c); }
};

TEST_CASE("Serialize POD") {
  POD pod{.a = 0x1234, .b = 0x56789ABC, .c = 0xDEF0123456789ABC};
  std::array<std::byte, 14> buffer;
  auto buffer_span = std::span(buffer);

  serialize_tuple(buffer_span, pod.to_tuple());

  POD deserialized = deserialize_tuple<POD>(buffer_span);
  CHECK(deserialized.a == pod.a);
  CHECK(deserialized.b == pod.b);
  CHECK(deserialized.c == pod.c);
}

TEST_CASE("Serialize POD with array") {
  PODWithArray pod{.a = 0x1234, .b = 0x56789ABC, .c = {0xDE, 0xF0, 0x12, 0x34}};
  std::array<std::byte, 10> buffer;
  auto buffer_span = std::span(buffer);

  serialize_tuple(buffer_span, pod.to_tuple());

  PODWithArray deserialized = deserialize_tuple<PODWithArray>(buffer_span);
  CHECK(deserialized.a == pod.a);
  CHECK(deserialized.b == pod.b);
  CHECK(deserialized.c == pod.c);
}

TEST_CASE("Serialize Non-POD") {
  NonPOD non_pod{.a = 0x1234, .b = 0x56789ABC, .c = "Hello"};
  std::array<std::byte, 20> buffer;
  auto buffer_span = std::span(buffer);

  static_assert(
      !invocable(LIFT(serialize_tuple), buffer_span, non_pod.to_tuple()),
      "serialize_tuple should not be invocable with Non-POD");
}

} // namespace serialization