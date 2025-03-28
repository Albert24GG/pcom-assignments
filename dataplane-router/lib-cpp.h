#pragma once

#include <array>
#include <filesystem>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <vector>

#define MAX_PACKET_LEN 1400
#define ROUTER_NUM_INTERFACES 3

/*
 * @brief Sends a packet on a specific interface.
 *
 * @param length - will be set to the total number of bytes received.
 * @param frame_data - region of memory in which the data will be copied; should
 *        have at least MAX_PACKET_LEN bytes allocated
 * @param interface - index of the output interface
 * Returns: the interface it has been received from.
 */
int send_to_link(size_t length, char *frame_data, size_t interface);

/*
 * @brief Receives a packet. Blocking function, blocks if there is no packet to
 * be received.
 *
 * @param frame_data - region of memory in which the data will be copied; should
 *        have at least MAX_PACKET_LEN bytes allocated
 * @param length - will be set to the total number of bytes received.
 * Returns: the interface it has been received from.
 */
size_t recv_from_any_link(char *frame_data, size_t *length);

/* Route table entry */
struct route_table_entry {
  uint32_t prefix;
  uint32_t next_hop;
  uint32_t mask;
  int interface;
};

/* ARP table entry when skipping the ARP exercise */
struct arp_table_entry {
  uint32_t ip;
  std::array<uint8_t, 6> mac;
};

char *get_interface_ip(int interface);

/**
 * @brief Get the interface mac object. The function writes
 * the MAC at the pointer mac. uint8_t *mac should be allocated.
 *
 * @param interface
 * @param mac
 */
void get_interface_mac(size_t interface, uint8_t *mac);

/**
 * @brief Homework infrastructure function.
 *
 * @param argc
 * @param argv
 */

/**
 * @brief IPv4 checksum per  RFC 791. To compute the checksum
 * of an IP header we must set the checksum to 0 beforehand.
 *
 * also works as ICMP checksum per RFC 792. To compute the checksum
 * of an ICMP header we must set the checksum to 0 beforehand.

 * @param data memory area to checksum
 * @param length in bytes
 */
uint16_t checksum(uint16_t *data, size_t length);

/**
 * @brief Parses a MAC address from a string and returns it as an array of 6
 * bytes.
 *
 * @param mac_str The MAC address string in the format "XX:XX:XX:XX:XX:XX".
 * @return An array of 6 bytes representing the MAC address.
 *
 * @throws std::runtime_error if the format is invalid.
 */
std::array<uint8_t, 6> parse_mac_address(std::string_view mac_str);

/**
 * @brief Parses a route table from path and returns a vector of
 * route_table_entry.
 *
 * @param filename Path to the file containing the route table.
 * @return A vector of route_table_entry structures.
 *
 * @throws std::runtime_error if the file cannot be opened or if the format
 *         is invalid.
 */
std::vector<route_table_entry>
parse_route_table(const std::filesystem::path &filename);

/**
  * @brief Parses a static mac table from path and returns a vector of
  * arp_table_entry.

  * @param filename Path to the file containing the ARP table.
  * @return A vector of arp_table_entry structures.

  * @throws std::runtime_error if the file cannot be opened or if the format
  *         is invalid.
*/
std::vector<arp_table_entry>
parse_arp_table(const std::filesystem::path &filename);

void init(char *argv[], int argc);

#define DIE(condition, message, ...)                                           \
  do {                                                                         \
    if ((condition)) {                                                         \
      fprintf(stderr, "[(%s:%d)]: " #message "\n", __FILE__, __LINE__,         \
              ##__VA_ARGS__);                                                  \
      perror("");                                                              \
      exit(1);                                                                 \
    }                                                                          \
  } while (0)
