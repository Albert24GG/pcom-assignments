#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

enum UdpPayloadType : uint8_t {
  INT = 0,
  SHORT_REAL,
  FLOAT,
  STRING,
  TOTAL_PAYLOAD_TYPES
};

template <UdpPayloadType T> struct UdpPayload;

template <> struct UdpPayload<UdpPayloadType::INT> {
  uint32_t value;
  uint8_t sign;

  static constexpr size_t size() { return sizeof(value) + sizeof(sign); }
};

template <> struct UdpPayload<UdpPayloadType::SHORT_REAL> {
  // Value representing the absolute value of the short real number multiplied
  // by 100
  uint16_t value;

  static constexpr size_t size() { return sizeof(value); }
};

template <> struct UdpPayload<UdpPayloadType::FLOAT> {
  // Value representing the absolute value of the number obtained by joining the
  // integer and fractional parts
  uint32_t value;
  uint8_t sign;
  // Absolute value of the negative exponent of 10 used to scale the number
  uint8_t exponent;

  static constexpr size_t size() {
    return sizeof(value) + sizeof(sign) + sizeof(exponent);
  }
};

template <> struct UdpPayload<UdpPayloadType::STRING> {
  std::string value;

  size_t size() const { return value.size(); }
};

using UdpPayloadVariant = std::variant<
    UdpPayload<UdpPayloadType::INT>, UdpPayload<UdpPayloadType::SHORT_REAL>,
    UdpPayload<UdpPayloadType::FLOAT>, UdpPayload<UdpPayloadType::STRING>>;

struct UdpMessage {
  std::string topic;
  UdpPayloadVariant payload;

  constexpr UdpPayloadType payload_type() const {
    return static_cast<UdpPayloadType>(payload.index());
  }
};

static constexpr size_t UDP_MSG_TOPIC_MAX_SIZE = 50;
static constexpr size_t UDP_MSG_PAYLOAD_MAX_SIZE = 1500;
static constexpr size_t UDP_MSG_MAX_SIZE =
    UDP_MSG_TOPIC_MAX_SIZE + UDP_MSG_PAYLOAD_MAX_SIZE + sizeof(UdpPayloadType);
// Minimum size of the payload for each payload type
static constexpr std::array<size_t, UdpPayloadType::TOTAL_PAYLOAD_TYPES>
    UDP_MSG_PAYLOAD_MIN_SIZE = {UdpPayload<UdpPayloadType::INT>::size(),
                                UdpPayload<UdpPayloadType::SHORT_REAL>::size(),
                                UdpPayload<UdpPayloadType::FLOAT>::size(), 1};
