#pragma once

#include <arpa/inet.h>
#include <cstdint>
extern "C" {
#include "lib.h"
#include "protocols.h"
}

constexpr uint16_t ETHERTYPE_ARP = 0x0806;
constexpr uint16_t ETHERTYPE_IP = 0x0800;

/**
 * @brief Get the interface IP address as a 32-bit unsigned integer.
 * @param interface The interface index.
 * @return The IP address of the interface as a 32-bit unsigned integer.
 *         The address is in network byte order (big-endian).
 */
inline uint32_t get_interface_ip_addr(int interface) {
  return inet_addr(get_interface_ip(interface));
}