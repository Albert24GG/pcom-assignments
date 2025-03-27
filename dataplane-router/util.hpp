#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>

namespace routing::util {

namespace concepts {
template <typename T> struct is_tuple : std::false_type {};

template <typename... Args>
struct is_tuple<std::tuple<Args...>> : std::true_type {};

template <typename T>
concept IsTuple = is_tuple<T>::value;

template <typename T>
concept HasToTuple = requires(T t) {
  { t.to_tuple() } -> IsTuple;
};

template <typename T>
concept TriviallyCopyable =
    std::is_trivially_copyable_v<std::remove_cvref_t<T>>;

template <HasToTuple Tuple, size_t BufferSize>
consteval bool deserializeable_tuple() {
  using TupleType = decltype(Tuple{}.to_tuple());
  using IndexSeq = std::make_index_sequence<std::tuple_size_v<TupleType>>;
  constexpr auto process_args =
      []<auto... I>(std::index_sequence<I...>) -> bool {
    return (TriviallyCopyable<std::tuple_element_t<I, TupleType>> && ...) &&
           (sizeof(std::tuple_element_t<I, TupleType>) + ...) == BufferSize;
  };
  return process_args(IndexSeq{});
}

template <typename T, size_t N>
concept DeserializeableToTuple = deserializeable_tuple<T, N>();

} // namespace concepts

using namespace concepts;

/**
 * @brief Converts a value from host byte order to network byte order.
 * This function is a no-op if the system is little-endian.
 *
 * @param value The value to convert.
 * @return The converted value in network byte order.
 *
 * @note This function is only available for integral types.
 */
constexpr auto host_to_network_order(std::integral auto value) {
  if constexpr (std::endian::native == std::endian::little) {
    constexpr auto size = sizeof(value);
    if constexpr (size == 1) {
      return value;
    } else if constexpr (size == 2) {
      return __builtin_bswap16(value);
    } else if constexpr (size == 4) {
      return __builtin_bswap32(value);
    } else if constexpr (size == 8) {
      return __builtin_bswap64(value);
    } else if constexpr (size == 16) {
      return __builtin_bswap128(value);
    } else {
      static_assert(size == 1 || size == 2 || size == 4 || size == 8 ||
                        size == 16,
                    "Unsupported type size for host_to_network_order");
    }
  } else {
    return value;
  }
}

/**
 * @brief Converts a value from network byte order to host byte order.
 * This function is a no-op if the system is little-endian.
 *
 * @param value The value to convert.
 * @return The converted value in host byte order.
 *
 * @note This function is only available for integral types.
 */
constexpr auto network_to_host_order(TriviallyCopyable auto &value) {
  return host_to_network_order(value);
}

/**
* @brief Serializes a trivially copyable field into a byte buffer.
* The function requires that the size of the buffer is sufficient to hold the
field.

* @tparam T The type of the field.
* @tparam N The size of the buffer.

* @param buffer The byte buffer to serialize into.
* @param field The field to serialize.
*/
template <TriviallyCopyable T, size_t N>
void serialize_field(std::span<std::byte, N> buffer, const T &field)
  requires(N >= sizeof(T))
{

  const auto field_span =
      std::span(reinterpret_cast<const std::byte *>(&field), sizeof(field));
  std::ranges::copy(field_span, buffer.begin());
}

/**
* @brief Serializes a field specified by a span into a byte buffer.
* The function requires that the field is trivially copyable
* and that the size of the buffer is sufficient to hold the field.

* @tparam T The type of the field.
* @tparam N The size of the buffer.
* @tparam M The size of the field.

* @param buffer The byte buffer to serialize into.
* @param field The field to serialize.
*/
template <TriviallyCopyable T, size_t N, size_t M>
void serialize_field(std::span<std::byte, N> buffer,
                     std::span<const T, M> field)
  requires(N >= sizeof(T) * M)
{
  auto field_bytes = std::as_bytes(field);
  std::ranges::copy(field_bytes, buffer.begin());
}

/**
* @brief Serializes the elements of a tuple into a byte buffer.
* The function requires that the tuple elements are trivially copyable
* and that the size of the buffer is sufficient to hold all elements.

* @tparam Args The types of the tuple elements.
* @tparam N The size of the buffer.

* @param buffer The byte buffer to serialize into.
* @param tuple The tuple to serialize.
*/
template <typename... Args, size_t N>
void serialize_tuple(std::span<std::byte, N> buffer,
                     const std::tuple<Args...> &tuple)
  requires requires {
    requires sizeof...(Args) > 0;
    requires(TriviallyCopyable<Args> && ...);
    requires N >= (sizeof(Args) + ...);
  }
{
  auto serialize = [&](const auto &...args) {
    size_t offset = 0;
    (
        [&] {
          auto buffer_span = buffer.subspan(offset, sizeof(args));
          serialize_field(buffer_span, args);
          offset += sizeof(args);
        }(),
        ...);
  };

  std::apply(serialize, tuple);
}

/**
  * @brief Deserializes a trivially copyable field from a byte buffer.
  * The function requires that the size of the buffer is sufficient to hold the
  * field and that the field is trivially copyable.

  * @tparam T The type of the field.
  * @tparam N The size of the buffer.

  * @param buffer The byte buffer to deserialize from.
  * @param field The field to deserialize.
*/
template <TriviallyCopyable T, size_t N>
void deserialize_field(std::span<const std::byte, N> buffer, T &field)
  requires(N == sizeof(T))
{
  auto field_span =
      std::span(reinterpret_cast<std::byte *>(&field), sizeof(field));
  std::ranges::copy(buffer, field_span.begin());
}

/**
 * @brief Deserializes a field specified by a span from a byte buffer.
 * The function requires that the span that represents the field is trivially
 * copyable and that the size of the buffer is sufficient to hold the field.

  * @tparam T The type of the field.
  * @tparam N The size of the buffer.
  * @tparam M The size of the field.

  * @param buffer The byte buffer to deserialize from.
  * @param field The field to deserialize.
 */
template <TriviallyCopyable T, size_t N, size_t M>
void deserialize_field(std::span<const std::byte, N> buffer,
                       std::span<T, M> field)
  requires(N == sizeof(T) * M)
{
  auto field_bytes = std::as_writable_bytes(field);
  std::ranges::copy(buffer, field_bytes.begin());
}

/**
* @brief Deserializes a buffer into its object representation.
* The function requires that the object

* @tparam Args The types of the tuple elements.
* @tparam N The size of the buffer.

* @param buffer The byte buffer to serialize into.
* @param tuple The tuple to serialize.
*/
template <typename T, size_t N>
T deserialize_tuple(std::span<std::byte, N> buffer)
  requires DeserializeableToTuple<T, N>
{
  T result{};
  auto deserialize = [&](auto &...args) {
    size_t offset = 0;
    (
        [&] {
          auto buffer_span =
              std::span<const std::byte, sizeof(args)>(buffer.subspan(offset));
          deserialize_field(buffer_span, args);
          offset += sizeof(args);
        }(),
        ...);
  };

  std::apply(deserialize, result.to_tuple());
  return result;
}

} // namespace routing::util