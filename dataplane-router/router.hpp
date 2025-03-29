#pragma once

#include "binary_trie.hpp"
#include "lib_wrapper.hpp"
#include "span.hpp"
#include "util.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace router {

class Router {
public:
  using iface_t = size_t;

  Router(const std::vector<route_table_entry> &rtable,
         const std::array<struct arp_table_entry, 6> &arp) {
    for (const auto &entry : arp) {
      std::array<uint8_t, 6> mac;
      std::copy(std::begin(entry.mac), std::end(entry.mac), mac.begin());
      arp_table.emplace(entry.ip, mac);
    }

    for (const auto &entry : rtable) {
      int prefix_len = util::countl_one(util::ntoh(entry.mask));
      uint32_t path = util::ntoh(entry.prefix);

      route_trie.insert(path, prefix_len, entry);
    }
  }

  void handle_frame(tcb::span<std::byte> frame, iface_t interface);

private:
  // Packet handlers
  void handle_arp_packet(tcb::span<std::byte> frame, iface_t interface);
  void handle_ip_packet(tcb::span<std::byte> frame, iface_t interface);
  void handle_local_ip_packet(tcb::span<std::byte> frame, iface_t interface);
  void handle_forward_ip_packet(tcb::span<std::byte> frame, iface_t interface);
  void send_frame(tcb::span<std::byte> frame, iface_t interface,
                  uint32_t dest_ip, uint16_t eth_type);

  struct interface_info {
    uint32_t ip;
    std::array<uint8_t, 6> mac;
  };
  // Helper functions
  interface_info get_interface_info(iface_t interface);
  uint32_t get_interface_ip(iface_t interface) {
    return get_interface_info(interface).ip;
  }
  std::array<uint8_t, 6> get_interface_mac(iface_t interface) {
    return get_interface_info(interface).mac;
  }
  bool is_for_this_router(uint32_t dest_ip, iface_t interface) {
    return (dest_ip == get_interface_ip(interface));
  }
  std::optional<std::pair<uint32_t, iface_t>> get_next_hop(uint32_t dest_ip) {
    auto entry = route_trie.longest_prefix_match(util::ntoh(dest_ip));
    if (entry) {
      uint32_t next_hop = entry->next_hop;
      iface_t interface = entry->interface;
      return std::make_pair(next_hop, interface);
    }
    return std::nullopt;
  }

  trie::BinaryTrie<uint32_t, route_table_entry> route_trie{};
  std::unordered_map<uint32_t, std::array<uint8_t, 6>> arp_table;

  std::unordered_map<iface_t, interface_info> interface_ip_map;
};

} // namespace router