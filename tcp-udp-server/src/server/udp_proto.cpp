#include "udp_proto.hpp"
#include "util.hpp"

#include <cstring>
#include <stdexcept>

void UdpPayloadInt::deserialize(UdpPayloadInt &payload, const std::byte *buffer,
                                size_t buffer_size) {
  if (buffer_size < UdpPayloadInt::MIN_SERIALIZED_SIZE) {
    throw std::invalid_argument(
        "Failed to deserialize UDP payload: buffer size is too small");
  }
  memcpy(&payload.sign, buffer, sizeof(payload.sign));
  buffer += sizeof(payload.sign);

  uint32_t value_network;
  memcpy(&value_network, buffer, sizeof(value_network));
  payload.value = ntoh(value_network);
}

void UdpPayloadShortReal::deserialize(UdpPayloadShortReal &payload,
                                      const std::byte *buffer,
                                      size_t buffer_size) {
  if (buffer_size < UdpPayloadShortReal::MIN_SERIALIZED_SIZE) {
    throw std::invalid_argument(
        "Failed to deserialize UDP payload: buffer size is too small");
  }

  uint16_t value_network;
  memcpy(&value_network, buffer, sizeof(value_network));
  payload.value = ntoh(value_network);
}

void UdpPayloadFloat::deserialize(UdpPayloadFloat &payload,
                                  const std::byte *buffer, size_t buffer_size) {
  if (buffer_size < UdpPayloadFloat::MIN_SERIALIZED_SIZE) {
    throw std::invalid_argument(
        "Failed to deserialize UDP payload: buffer size is too small");
  }

  memcpy(&payload.sign, buffer, sizeof(payload.sign));
  buffer += sizeof(payload.sign);

  uint32_t value_network;
  memcpy(&value_network, buffer, sizeof(value_network));
  payload.value = ntoh(value_network);
  buffer += sizeof(value_network);

  memcpy(&payload.exponent, buffer, sizeof(payload.exponent));
}

void UdpPayloadString::deserialize(UdpPayloadString &payload,
                                   const std::byte *buffer,
                                   size_t buffer_size) {
  if (buffer_size < UdpPayloadString::MIN_SERIALIZED_SIZE) {
    throw std::invalid_argument(
        "Failed to deserialize UDP payload: buffer size is too small");
  }

  size_t max_str_len =
      std::min(buffer_size, UdpPayloadString::MAX_SERIALIZED_SIZE);

  size_t str_len = strnlen(reinterpret_cast<const char *>(buffer), max_str_len);
  memcpy(payload.value.data(), buffer, str_len);
  payload.value[str_len] = '\0';

  payload.value_size = str_len;
}

void UdpMessage::deserialize(UdpMessage &msg, const std::byte *buffer,
                             size_t buffer_size) {

  if (buffer_size < UdpMessage::MIN_SERIALIZED_SIZE) {
    throw std::invalid_argument(
        "Failed to deserialize UDP message: buffer size is too small");
  }

  size_t topic_len =
      strnlen(reinterpret_cast<const char *>(buffer), UDP_MSG_TOPIC_SIZE);

  memcpy(msg.topic.data(), buffer, topic_len);
  msg.topic[topic_len] = '\0';
  msg.topic_size = topic_len;
  buffer += UDP_MSG_TOPIC_SIZE;

  UdpPayloadType payload_type = [buffer] {
    uint8_t type;
    memcpy(&type, buffer, sizeof(type));
    return static_cast<UdpPayloadType>(type);
  }();
  buffer += sizeof(UdpPayloadType);

  buffer_size -= UDP_MSG_TOPIC_SIZE + sizeof(UdpPayloadType);

  switch (payload_type) {
  case UdpPayloadType::INT:
    msg.payload.emplace<UdpPayloadInt>();
    UdpPayloadInt::deserialize(std::get<UdpPayloadInt>(msg.payload), buffer,
                               buffer_size);
    break;
  case UdpPayloadType::SHORT_REAL:
    msg.payload.emplace<UdpPayloadShortReal>();
    UdpPayloadShortReal::deserialize(std::get<UdpPayloadShortReal>(msg.payload),
                                     buffer, buffer_size);
    break;
  case UdpPayloadType::FLOAT:
    msg.payload.emplace<UdpPayloadFloat>();
    UdpPayloadFloat::deserialize(std::get<UdpPayloadFloat>(msg.payload), buffer,
                                 buffer_size);
    break;
  case UdpPayloadType::STRING:
    msg.payload.emplace<UdpPayloadString>();
    UdpPayloadString::deserialize(std::get<UdpPayloadString>(msg.payload),
                                  buffer, buffer_size);
    break;
  default:
    throw std::invalid_argument(
        "Failed to deserialize UDP message: unknown payload type");
  }
}
