#pragma once

#include <arpa/inet.h>
#include <cstdint>
extern "C" {
#include "lib.h"
#include "protocols.h"
}

constexpr uint16_t ETHERTYPE_ARP = 0x0806;
constexpr uint16_t ETHERTYPE_IP = 0x0800;
constexpr size_t ETHER_HDR_SIZE = sizeof(struct ether_hdr);

constexpr uint16_t ARP_OPCODE_REQUEST = 1;
constexpr uint16_t ARP_OPCODE_REPLY = 2;
constexpr uint16_t ARP_HW_TYPE_ETHERNET = 1;
constexpr uint16_t ARP_PROTO_TYPE_IP = 0x0800;
constexpr uint8_t ARP_HW_LEN = 6;
constexpr uint8_t ARP_PROTO_LEN = 4;
constexpr size_t ARP_HDR_SIZE = sizeof(struct arp_hdr);

constexpr size_t IP_HDR_SIZE = sizeof(struct ip_hdr);
constexpr size_t ICMP_HDR_SIZE = sizeof(struct icmp_hdr);

/**
 * @brief Get the interface IP address as a 32-bit unsigned integer.
 * @param interface The interface index.
 * @return The IP address of the interface as a 32-bit unsigned integer.
 *         The address is in network byte order (big-endian).
 */
inline uint32_t get_interface_ip_addr(int interface) {
  return inet_addr(get_interface_ip(interface));
}