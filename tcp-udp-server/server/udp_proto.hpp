#pragma once

#include "proto_utils.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>

// ##############################################################################
// # Constants
// ##############################################################################

static constexpr size_t UDP_MSG_TOPIC_SIZE = 50;
static constexpr size_t UDP_PAYLOAD_STRING_MAX_SIZE = 1500;

// ##############################################################################
// # UdpMessage
// ##############################################################################

enum class UdpPayloadType : uint8_t {
  INT = 0,
  SHORT_REAL,
  FLOAT,
  STRING,
  TOTAL_PAYLOAD_TYPES
};

struct UdpPayloadInt {
  uint32_t value;
  uint8_t sign;

  /**
   * @brief Deserializes the INT payload from a byte buffer.
   *
   * @param payload The INT payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(UdpPayloadInt &payload, const std::byte *buffer,
                          size_t buffer_size);

  static constexpr size_t MAX_SERIALIZED_SIZE = sizeof(sign) + sizeof(value);
  static constexpr size_t MIN_SERIALIZED_SIZE = sizeof(sign) + sizeof(value);
};

struct UdpPayloadShortReal {
  // Value representing the absolute value of the short real number multiplied
  // by 100
  uint16_t value;

  /**
   * @brief Deserializes the SHORT_REAL payload from a byte buffer.
   *
   * @param payload The SHORT_REAL payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(UdpPayloadShortReal &payload, const std::byte *buffer,
                          size_t buffer_size);

  static constexpr size_t MAX_SERIALIZED_SIZE = sizeof(value);
  static constexpr size_t MIN_SERIALIZED_SIZE = sizeof(value);
};

struct UdpPayloadFloat {
  // Value representing the absolute value of the number obtained by joining the
  // integer and fractional parts
  uint32_t value;
  uint8_t sign;
  // Absolute value of the negative exponent of 10 used to scale the number
  uint8_t exponent;

  /**
   * @brief Deserializes the FLOAT payload from a byte buffer.
   *
   * @param payload The FLOAT payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(UdpPayloadFloat &payload, const std::byte *buffer,
                          size_t buffer_size);

  static constexpr size_t MAX_SERIALIZED_SIZE =
      sizeof(sign) + sizeof(value) + sizeof(exponent);
  static constexpr size_t MIN_SERIALIZED_SIZE =
      sizeof(sign) + sizeof(value) + sizeof(exponent);
};

struct UdpPayloadString {
  std::array<char, UDP_PAYLOAD_STRING_MAX_SIZE + 1> value{};
  uint16_t value_size{};

  /**
   * @brief Deserializes the STRING payload from a byte buffer.
   *
   * @param payload The STRING payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(UdpPayloadString &payload, const std::byte *buffer,
                          size_t buffer_size);

  // Only the string is serialized, the size variable is not included
  static constexpr size_t MIN_SERIALIZED_SIZE = 1;
  static constexpr size_t MAX_SERIALIZED_SIZE = UDP_PAYLOAD_STRING_MAX_SIZE;
};

using UdpPayloadVariant = std::variant<UdpPayloadInt, UdpPayloadShortReal,
                                       UdpPayloadFloat, UdpPayloadString>;

struct UdpMessage {
  std::array<char, UDP_MSG_TOPIC_SIZE + 1> topic{};
  uint8_t topic_size{};
  UdpPayloadVariant payload;

  /**
   * @brief Deserializes the message from a byte buffer.
   *
   * @param msg The message to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(UdpMessage &msg, const std::byte *buffer,
                          size_t buffer_size);

  constexpr UdpPayloadType payload_type() const {
    return static_cast<UdpPayloadType>(payload.index());
  }

  static constexpr size_t MAX_SERIALIZED_SIZE =
      UDP_MSG_TOPIC_SIZE + sizeof(UdpPayloadType) +
      max_serialized_size<UdpPayloadVariant>();
  static constexpr size_t MIN_SERIALIZED_SIZE =
      UDP_MSG_TOPIC_SIZE + sizeof(UdpPayloadType) +
      min_serialized_size<UdpPayloadVariant>();
};
