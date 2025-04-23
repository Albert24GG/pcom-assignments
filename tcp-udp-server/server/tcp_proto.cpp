#include "tcp_proto.hpp"
#include "../server/util.hpp"
#include <cstring>
#include <stdexcept>
#include <variant>

// ##############################################################################
// # Helper functions
// ##############################################################################

namespace {
template <typename V> void serialize_variant(V &&variant, std::byte *buffer) {
  std::visit(
      [&buffer](auto &&arg) {
        using T = std::decay_t<decltype(arg)>;
        T::serialize(arg, buffer);
      },
      std::forward<V>(variant));
}
} // namespace

// ####################################################################
// # TcpRequest implementations
// ####################################################################

void TcpRequestPayloadId::set(const char *id_data, size_t size) {
  if (size > TCP_CLIENT_ID_MAX_SIZE) {
    throw std::invalid_argument("ID size exceeds maximum limit");
  }
  memcpy(id.data(), id_data, size);
  id_size = size;
}

void TcpRequestPayloadId::serialize(const TcpRequestPayloadId &payload,
                                    std::byte *buffer) {
  if (payload.id_size > TCP_CLIENT_ID_MAX_SIZE) {
    throw std::invalid_argument(
        "Failed to serialize tcp request ID: size exceeds maximum limit");
  }

  uint8_t cast_id_size = static_cast<uint8_t>(payload.id_size);
  memcpy(buffer, &cast_id_size, sizeof(cast_id_size));
  buffer += sizeof(cast_id_size);

  memcpy(buffer, payload.id.data(), payload.id_size);
}

void TcpRequestPayloadId::deserialize(TcpRequestPayloadId &payload,
                                      const std::byte *buffer,
                                      size_t buffer_size) {
  if (buffer_size < sizeof(uint8_t)) {
    throw std::invalid_argument(
        "Failed to deserialize tcp request ID size: buffer size is too small");
  }

  uint8_t id_size{};
  memcpy(&id_size, buffer, sizeof(id_size));
  buffer += sizeof(id_size);
  buffer_size -= sizeof(id_size);

  if (id_size > TCP_CLIENT_ID_MAX_SIZE) {
    throw std::invalid_argument(
        "Failed to deserialize tcp request ID: ID size exceeds maximum limit");
  }
  if (id_size > buffer_size) {
    throw std::invalid_argument(
        "Failed to deserialize tcp request ID data: buffer size is too small");
  }

  memcpy(payload.id.data(), buffer, id_size);
  payload.id[id_size] = '\0';
  payload.id_size = id_size;
}

void TcpRequestPayloadTopic::set(const char *topic_data, size_t size) {
  if (size > TCP_RESP_TOPIC_MAX_SIZE) {
    throw std::invalid_argument("TOPIC size exceeds maximum limit");
  }
  memcpy(topic.data(), topic_data, size);
  topic_size = size;
}

void TcpRequestPayloadTopic::serialize(const TcpRequestPayloadTopic &payload,
                                       std::byte *buffer) {
  if (payload.topic_size > TCP_RESP_TOPIC_MAX_SIZE) {
    throw std::invalid_argument(
        "Failed to serialize topic: size exceeds maximum limit");
  }

  uint8_t cast_topic_size = static_cast<uint8_t>(payload.topic_size);
  memcpy(buffer, &cast_topic_size, sizeof(cast_topic_size));
  buffer += sizeof(cast_topic_size);

  memcpy(buffer, payload.topic.data(), payload.topic_size);
}

void TcpRequestPayloadTopic::deserialize(TcpRequestPayloadTopic &topic,
                                         const std::byte *buffer,
                                         size_t buffer_size) {
  if (buffer_size < sizeof(uint8_t)) {
    throw std::invalid_argument(
        "Failed to deserialize topic size: buffer size is too small");
  }

  uint8_t topic_size{};
  memcpy(&topic_size, buffer, sizeof(topic_size));
  buffer += sizeof(topic_size);
  buffer_size -= sizeof(topic_size);

  if (topic_size > TCP_RESP_TOPIC_MAX_SIZE) {
    throw std::invalid_argument(
        "Failed to deserialize topic: topic size exceeds maximum limit");
  }
  if (topic_size > buffer_size) {
    throw std::invalid_argument(
        "Failed to deserialize topic data: buffer size is too small");
  }

  memcpy(topic.topic.data(), buffer, topic_size);
  topic.topic[topic_size] = '\0';
  topic.topic_size = topic_size;
}

void TcpRequest::serialize(const TcpRequest &request, std::byte *buffer) {
  uint8_t cast_type = static_cast<uint8_t>(request.type);
  memcpy(buffer, &cast_type, sizeof(cast_type));
  buffer += sizeof(type);

  serialize_variant(request.payload, buffer);
}

void TcpRequest::deserialize(TcpRequest &request, const std::byte *buffer,
                             size_t buffer_size) {
  if (buffer_size < sizeof(TcpRequestType)) {
    throw std::invalid_argument(
        "Failed to deserialize request type: buffer size is too small");
  }

  uint8_t type{};
  memcpy(&type, buffer, sizeof(type));
  buffer += sizeof(type);
  buffer_size -= sizeof(type);

  request.type = static_cast<TcpRequestType>(type);

  switch (request.type) {
  case TcpRequestType::CONNECT:
    request.payload.emplace<TcpRequestPayloadId>();
    TcpRequestPayloadId::deserialize(
        std::get<TcpRequestPayloadId>(request.payload), buffer, buffer_size);
    break;
  case TcpRequestType::SUBSCRIBE:
  case TcpRequestType::UNSUBSCRIBE:
    request.payload.emplace<TcpRequestPayloadTopic>();
    TcpRequestPayloadTopic::deserialize(
        std::get<TcpRequestPayloadTopic>(request.payload), buffer, buffer_size);
    break;
  default:
    throw std::invalid_argument("Failed to deserialize request: unknown type");
  }
}

// ####################################################################
// # TcpResponse implementations
// ####################################################################

void TcpResponsePayloadInt::serialize(const TcpResponsePayloadInt &payload,
                                      std::byte *buffer) {
  memcpy(buffer, &payload.sign, sizeof(sign));
  buffer += sizeof(sign);

  uint32_t value_network = hton(payload.value);
  memcpy(buffer, &value_network, sizeof(value_network));
}

void TcpResponsePayloadInt::deserialize(TcpResponsePayloadInt &payload,
                                        const std::byte *buffer,
                                        size_t buffer_size) {
  if (buffer_size < sizeof(sign) + sizeof(value)) {
    throw std::invalid_argument(
        "Failed to deserialize tcp response INT: buffer size is too small");
  }

  memcpy(&payload.sign, buffer, sizeof(sign));
  buffer += sizeof(sign);

  uint32_t value_network;
  memcpy(&value_network, buffer, sizeof(value_network));
  payload.value = ntoh(value_network);
}

void TcpResponsePayloadShortReal::serialize(
    const TcpResponsePayloadShortReal &payload, std::byte *buffer) {
  uint16_t value_network = hton(payload.value);
  memcpy(buffer, &value_network, sizeof(value_network));
}

void TcpResponsePayloadShortReal::deserialize(
    TcpResponsePayloadShortReal &payload, const std::byte *buffer,
    size_t buffer_size) {
  if (buffer_size < sizeof(value)) {
    throw std::invalid_argument("Failed to deserialize tcp response "
                                "SHORT_REAL: buffer size is too small");
  }

  uint16_t value_network;
  memcpy(&value_network, buffer, sizeof(value_network));
  payload.value = ntoh(value_network);
}

void TcpResponsePayloadFloat::serialize(const TcpResponsePayloadFloat &payload,
                                        std::byte *buffer) {
  memcpy(buffer, &payload.sign, sizeof(sign));
  buffer += sizeof(sign);

  uint32_t value_network = hton(payload.value);
  memcpy(buffer, &value_network, sizeof(value_network));
  buffer += sizeof(value_network);

  memcpy(buffer, &payload.exponent, sizeof(exponent));
}

void TcpResponsePayloadFloat::deserialize(TcpResponsePayloadFloat &payload,
                                          const std::byte *buffer,
                                          size_t buffer_size) {
  if (buffer_size < sizeof(sign) + sizeof(value) + sizeof(exponent)) {
    throw std::invalid_argument(
        "Failed to deserialize tcp response FLOAT: buffer size is too small");
  }

  memcpy(&payload.sign, buffer, sizeof(sign));
  buffer += sizeof(sign);

  uint32_t value_network;
  memcpy(&value_network, buffer, sizeof(value_network));
  payload.value = ntoh(value_network);
  buffer += sizeof(value_network);

  memcpy(&payload.exponent, buffer, sizeof(exponent));
}

void TcpResponsePayloadString::serialize(
    const TcpResponsePayloadString &payload, std::byte *buffer) {
  if (payload.value_size > TCP_RESP_STRING_MAX_SIZE) {
    throw std::invalid_argument(
        "Failed to serialize tcp response STRING: size exceeds maximum limit");
  }

  uint16_t cast_value_size_network =
      hton(static_cast<uint16_t>(payload.value_size));
  memcpy(buffer, &cast_value_size_network, sizeof(cast_value_size_network));
  buffer += sizeof(cast_value_size_network);

  memcpy(buffer, payload.value.data(), payload.value_size);
}

void TcpResponsePayloadString::deserialize(TcpResponsePayloadString &payload,
                                           const std::byte *buffer,
                                           size_t buffer_size) {
  if (buffer_size < sizeof(uint16_t)) {
    throw std::invalid_argument("Failed to deserialize tcp response STRING "
                                "size: buffer size is too small");
  }

  uint16_t value_size_network;
  memcpy(&value_size_network, buffer, sizeof(value_size_network));
  buffer += sizeof(value_size_network);
  buffer_size -= sizeof(value_size_network);
  uint16_t value_size = ntoh(value_size_network);

  if (value_size > TCP_RESP_STRING_MAX_SIZE) {
    throw std::invalid_argument(
        "Failed to deserialize tcp response STRING: size exceeds maximum "
        "limit");
  }
  if (value_size > buffer_size) {
    throw std::invalid_argument(
        "Failed to deserialize tcp response STRING data: buffer size is too "
        "small");
  }

  memcpy(payload.value.data(), buffer, value_size);
  payload.value[value_size] = '\0';
  payload.value_size = value_size;
}

void TcpResponsePayloadString::set(const char *value_data, size_t size) {
  if (size > TCP_RESP_STRING_MAX_SIZE) {
    throw std::invalid_argument("STRING size exceeds maximum limit");
  }
  memcpy(value.data(), value_data, size);
  value_size = size;
}

void TcpResponse::serialize(const TcpResponse &response, std::byte *buffer) {
  memcpy(buffer, &response.udp_client_ip, sizeof(udp_client_ip));
  buffer += sizeof(udp_client_ip);

  uint16_t udp_client_port_network = hton(response.udp_client_port);
  memcpy(buffer, &udp_client_port_network, sizeof(udp_client_port_network));
  buffer += sizeof(udp_client_port_network);

  uint8_t cast_topic_size = static_cast<uint8_t>(response.topic_size);
  memcpy(buffer, &cast_topic_size, sizeof(cast_topic_size));
  buffer += sizeof(cast_topic_size);

  memcpy(buffer, response.topic.data(), response.topic_size);
  buffer += response.topic_size;

  uint8_t type = static_cast<uint8_t>(response.payload_type());
  memcpy(buffer, &type, sizeof(type));
  buffer += sizeof(type);

  serialize_variant(response.payload, buffer);
}

void TcpResponse::deserialize(TcpResponse &response, const std::byte *buffer,
                              size_t buffer_size) {
  if (buffer_size < sizeof(udp_client_ip) + sizeof(udp_client_port) +
                        sizeof(topic_size) + sizeof(TcpResponsePayloadType)) {
    throw std::invalid_argument(
        "Failed to deserialize tcp response: buffer size is too small");
  }

  memcpy(&response.udp_client_ip, buffer, sizeof(udp_client_ip));
  buffer += sizeof(udp_client_ip);

  uint16_t udp_client_port_network;
  memcpy(&udp_client_port_network, buffer, sizeof(udp_client_port_network));
  response.udp_client_port = ntoh(udp_client_port_network);
  buffer += sizeof(udp_client_port_network);

  uint8_t topic_size;
  memcpy(&topic_size, buffer, sizeof(topic_size));
  response.topic_size = topic_size;
  buffer += sizeof(topic_size);

  buffer_size -=
      sizeof(udp_client_ip) + sizeof(udp_client_port) + sizeof(topic_size);

  if (buffer_size < response.topic_size) {
    throw std::invalid_argument("Failed to deserialize tcp response TOPIC "
                                "data: buffer size is too small");
  }

  memcpy(response.topic.data(), buffer, topic_size);
  response.topic[topic_size] = '\0';
  buffer += response.topic_size;

  TcpResponsePayloadType payload_type = [buffer] {
    uint8_t type;
    memcpy(&type, buffer, sizeof(type));
    return static_cast<TcpResponsePayloadType>(type);
  }();
  buffer += sizeof(payload_type);

  buffer_size -= topic_size + sizeof(payload_type);

  switch (payload_type) {
  case TcpResponsePayloadType::INT:
    response.payload.emplace<TcpResponsePayloadInt>();
    TcpResponsePayloadInt::deserialize(
        std::get<TcpResponsePayloadInt>(response.payload), buffer, buffer_size);
    break;
  case TcpResponsePayloadType::SHORT_REAL:
    response.payload.emplace<TcpResponsePayloadShortReal>();
    TcpResponsePayloadShortReal::deserialize(
        std::get<TcpResponsePayloadShortReal>(response.payload), buffer,
        buffer_size);
    break;
  case TcpResponsePayloadType::FLOAT:
    response.payload.emplace<TcpResponsePayloadFloat>();
    TcpResponsePayloadFloat::deserialize(
        std::get<TcpResponsePayloadFloat>(response.payload), buffer,
        buffer_size);
    break;
  case TcpResponsePayloadType::STRING:
    response.payload.emplace<TcpResponsePayloadString>();
    TcpResponsePayloadString::deserialize(
        std::get<TcpResponsePayloadString>(response.payload), buffer,
        buffer_size);
    break;
  default:
    throw std::invalid_argument(
        "Failed to deserialize tcp response: unknown payload type");
  }
}

// ####################################################################
// # TcpMessage implementations
// ####################################################################

void TcpMessage::serialize(const TcpMessage &message, std::byte *buffer) {
  uint8_t payload_type = static_cast<uint8_t>(message.payload_type());
  memcpy(buffer, &payload_type, sizeof(payload_type));
  buffer += sizeof(payload_type);

  uint16_t payload_size = std::visit(
      [](auto &&arg) { return arg.serialized_size(); }, message.payload);
  uint16_t payload_size_network = hton(payload_size);
  memcpy(buffer, &payload_size_network, sizeof(payload_size_network));
  buffer += sizeof(payload_size_network);

  serialize_variant(message.payload, buffer);
}
