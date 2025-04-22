#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <utility>
#include <variant>

// ##############################################################################
// # Helper functions
// ##############################################################################

template <typename PayloadVariant, auto... Idx>
constexpr size_t max_serialized_size_impl(std::index_sequence<Idx...>) {
  return std::max(
      {std::variant_alternative_t<Idx,
                                  PayloadVariant>::MAX_SERIALIZED_SIZE...});
}

// Helper function to get the maximum serialized size of a payload variant
template <typename PayloadVariant> constexpr size_t max_serialized_size() {
  return max_serialized_size_impl<PayloadVariant>(
      std::make_index_sequence<std::variant_size_v<PayloadVariant>>{});
}

// ##############################################################################
// # Constants
// ##############################################################################

static constexpr size_t TCP_CLIENT_ID_MAX_SIZE = 10;
static constexpr size_t TCP_REQ_PAYLOAD_MAX_SIZE = 50;
static constexpr size_t TCP_RESP_TOPIC_MAX_SIZE = 50;
static constexpr size_t TCP_RESP_STRING_MAX_SIZE = 1500;

// ##############################################################################
// # TcpRequest
// ##############################################################################

enum class TcpRequestPayloadType : uint8_t {
  ID = 0,
  TOPIC,
  TOTAL_PAYLOAD_TYPES
};

struct TcpRequestPayloadId {
  std::array<char, TCP_CLIENT_ID_MAX_SIZE + 1> id{};
  uint8_t id_size{};

  /**
   * @brief Sets the ID value and its size.
   * The function copies `size` bytes from `id_data` to the `id` buffer and
   * updates the `id_size`.
   *
   * @param id_data The ID data buffer to copy from.
   * @param size The size of the ID data in bytes.
   *
   * @throws std::invalid_argument if the size exceeds the maximum allowed size.
   */
  void set(const char *id_data, size_t size);

  /**
   * @brief Serializes the ID payload into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param payload The ID payload to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpRequestPayloadId &payload, std::byte *buffer);

  /**
   * @brief Deserializes the ID payload from a byte buffer.
   *
   * @param payload The ID payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpRequestPayloadId &payload, const std::byte *buffer,
                          size_t buffer_size);

  constexpr size_t serialized_size() const { return sizeof(id_size) + id_size; }

  static constexpr size_t MAX_SERIALIZED_SIZE =
      sizeof(id_size) + TCP_CLIENT_ID_MAX_SIZE;
};

struct TcpRequestPayloadTopic {
  std::array<char, TCP_RESP_TOPIC_MAX_SIZE + 1> topic{};
  uint8_t topic_size{};

  /**
   * @brief Sets the topic value and its size.
   * The function copies `size` bytes from `topic_data` to the `topic` buffer
   * and updates the `topic_size`.
   *
   * @param topic_data The topic data buffer to copy from.
   * @param size The size of the topic data in bytes.
   *
   * @throws std::invalid_argument if the size exceeds the maximum allowed size.
   */
  void set(const char *topic_data, size_t size);

  /**
   * @brief Serializes the topic payload into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param payload The topic payload to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpRequestPayloadTopic &payload,
                        std::byte *buffer);

  /**
   * @brief Deserializes the topic payload from a byte buffer.
   *
   * @param payload The topic payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpRequestPayloadTopic &payload,
                          const std::byte *buffer, size_t buffer_size);

  constexpr size_t serialized_size() const {
    return sizeof(topic_size) + topic_size;
  }

  static constexpr size_t MAX_SERIALIZED_SIZE =
      sizeof(topic_size) + TCP_RESP_TOPIC_MAX_SIZE;
};

enum TcpRequestType : uint8_t {
  CONNECT = 0,
  SUBSCRIBE,
  UNSUBSCRIBE,
  TOTAL_REQUEST_TYPES
};

using TcpRequestPayloadVariant =
    std::variant<TcpRequestPayloadId, TcpRequestPayloadTopic>;

struct TcpRequest {
  TcpRequestPayloadVariant payload;
  TcpRequestType type;

  /**
   * @brief Serializes the request into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param request The request to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpRequest &request, std::byte *buffer);

  /**
   * @brief Deserializes the request from a byte buffer.
   *
   * @param request The request to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpRequest &request, const std::byte *buffer,
                          size_t buffer_size);

  constexpr auto payload_type() const -> TcpRequestPayloadType {
    return static_cast<TcpRequestPayloadType>(payload.index());
  }

  constexpr size_t serialized_size() const {
    return sizeof(type) +
           std::visit([](auto &&arg) { return arg.serialized_size(); },
                      payload);
  }

  static constexpr size_t MAX_SERIALIZED_SIZE =
      sizeof(type) + max_serialized_size<TcpRequestPayloadVariant>();
};

// ##############################################################################
// # TcpResponse
// ##############################################################################

enum class TcpResponsePayloadType : uint8_t {
  INT = 0,
  SHORT_REAL,
  FLOAT,
  STRING,
  TOTAL_PAYLOAD_TYPES
};

struct TcpResponsePayloadInt {
  uint32_t value;
  uint8_t sign;

  /**
   * @brief Serializes the INT payload into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param payload The INT payload to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpResponsePayloadInt &payload,
                        std::byte *buffer);

  /**
   * @brief Deserializes the INT payload from a byte buffer.
   *
   * @param payload The INT payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpResponsePayloadInt &payload,
                          const std::byte *buffer, size_t buffer_size);

  static constexpr size_t serialized_size() {
    return sizeof(sign) + sizeof(value);
  }

  static constexpr size_t MAX_SERIALIZED_SIZE = sizeof(sign) + sizeof(value);
};

struct TcpResponsePayloadShortReal {
  // Value representing the absolute value of the short real number multiplied
  // by 100
  uint16_t value;

  /**
   * @brief Serializes the SHORT_REAL payload into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param payload The SHORT_REAL payload to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpResponsePayloadShortReal &payload,
                        std::byte *buffer);

  /**
   * @brief Deserializes the SHORT_REAL payload from a byte buffer.
   *
   * @param payload The SHORT_REAL payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpResponsePayloadShortReal &payload,
                          const std::byte *buffer, size_t buffer_size);

  static constexpr size_t serialized_size() { return sizeof(value); }

  static constexpr size_t MAX_SERIALIZED_SIZE = sizeof(value);
};

struct TcpResponsePayloadFloat {
  // Value representing the absolute value of the number obtained by joining the
  // integer and fractional parts
  uint32_t value;
  uint8_t sign;
  // Absolute value of the negative exponent of 10 used to scale the number
  uint8_t exponent;

  /**
   * @brief Serializes the FLOAT payload into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param payload The FLOAT payload to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpResponsePayloadFloat &payload,
                        std::byte *buffer);

  /**
   * @brief Deserializes the FLOAT payload from a byte buffer.
   *
   * @param payload The FLOAT payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpResponsePayloadFloat &payload,
                          const std::byte *buffer, size_t buffer_size);

  static constexpr size_t serialized_size() {
    return sizeof(sign) + sizeof(value) + sizeof(exponent);
  }

  static constexpr size_t MAX_SERIALIZED_SIZE =
      sizeof(sign) + sizeof(value) + sizeof(exponent);
};

struct TcpResponsePayloadString {
  std::array<char, TCP_RESP_STRING_MAX_SIZE + 1> value{};
  uint16_t value_size{};

  /**
   * @brief Sets the STRING value and its size.
   * The function copies `size` bytes from `value_data` to the `value` buffer
   * and updates the `value_size`.
   *
   * @param value_data The STRING data buffer to copy from.
   * @param size The size of the STRING data in bytes.
   *
   * @throws std::invalid_argument if the size exceeds the maximum allowed size.
   */
  void set(const char *value_data, size_t size);

  /**
   * @brief Serializes the STRING payload into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param payload The STRING payload to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpResponsePayloadString &payload,
                        std::byte *buffer);

  /**
   * @brief Deserializes the STRING payload from a byte buffer.
   *
   * @param payload The STRING payload to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpResponsePayloadString &payload,
                          const std::byte *buffer, size_t buffer_size);

  constexpr size_t serialized_size() const {
    return sizeof(value_size) + value_size;
  }

  static constexpr size_t MAX_SERIALIZED_SIZE =
      sizeof(value_size) + TCP_RESP_STRING_MAX_SIZE;
};

using TcpResponsePayloadVariant =
    std::variant<TcpResponsePayloadInt, TcpResponsePayloadShortReal,
                 TcpResponsePayloadFloat, TcpResponsePayloadString>;

struct TcpResponse {
  // Ip must be in network byte order
  uint32_t udp_client_ip;
  // Port must be in host byte order
  uint16_t udp_client_port;

  std::array<char, TCP_RESP_TOPIC_MAX_SIZE + 1> topic{};
  uint8_t topic_size{};
  TcpResponsePayloadVariant payload;

  /**
   * @brief Serializes the response into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param response The response to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpResponse &response, std::byte *buffer);

  /**
   * @brief Deserializes the response from a byte buffer.
   *
   * @param response The response to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpResponse &response, const std::byte *buffer,
                          size_t buffer_size);

  constexpr auto payload_type() const -> TcpResponsePayloadType {
    return static_cast<TcpResponsePayloadType>(payload.index());
  }

  constexpr size_t serialized_size() const {
    return sizeof(udp_client_ip) + sizeof(udp_client_port) +
           sizeof(topic_size) + topic_size + sizeof(TcpResponsePayloadType) +
           std::visit([](auto &&arg) { return arg.serialized_size(); },
                      payload);
  }

  static constexpr size_t MAX_SERIALIZED_SIZE =
      sizeof(udp_client_ip) + sizeof(udp_client_port) + sizeof(topic_size) +
      TCP_RESP_TOPIC_MAX_SIZE + sizeof(TcpResponsePayloadType) +
      max_serialized_size<TcpResponsePayloadVariant>();
};

// ##############################################################################
// # TcpMessage
// ##############################################################################

enum class TcpMessageType : uint8_t {
  REQUEST = 0,
  RESPONSE,
  TOTAL_MESSAGE_TYPES
};

using TcpMessageVariant = std::variant<TcpRequest, TcpResponse>;

struct TcpMessage {
  TcpMessageVariant payload;
  uint32_t payload_size;

  /**
   * @brief Serializes the message into a byte buffer.
   * The caller is responsible for ensuring that the buffer is large enough to
   * hold the serialized data.
   * The required size is given by `serialized_size()`.
   *
   * @param message The message to serialize.
   * @param buffer The byte buffer to store the serialized data.
   *
   * @throws std::invalid_argument if the serialization fails.
   */
  static void serialize(const TcpMessage &message, std::byte *buffer);

  /**
   * @brief Deserializes the message from a byte buffer.
   *
   * @param message The message to deserialize into.
   * @param buffer The byte buffer containing the serialized data.
   * @param buffer_size The size of the byte buffer.
   *
   * @throws std::invalid_argument if the deserialization fails.
   */
  static void deserialize(TcpMessage &message, const std::byte *buffer,
                          size_t buffer_size);

  constexpr auto payload_type() const -> TcpMessageType {
    return static_cast<TcpMessageType>(payload.index());
  }

  constexpr size_t serialized_size() const {
    return sizeof(TcpMessageType) + sizeof(payload_size) +
           std::visit([](auto &&arg) { return arg.serialized_size(); },
                      payload);
  }

  static constexpr size_t MAX_SERIALIZED_SIZE =
      sizeof(TcpMessageType) + sizeof(payload_size) +
      max_serialized_size<TcpMessageVariant>();
};
