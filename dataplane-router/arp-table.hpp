#pragma once

#include "common.hpp"
#include "lib_wrapper.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace router::arp {

struct PendingPacket {
  iface_t next_hop_iface;
  std::vector<std::byte> frame;
};

struct ArpTableEntry {
  uint32_t ip;
  std::array<uint8_t, 6> mac;
};

class ArpTable {
public:
  void add_entry(ArpTableEntry entry) {
    arp_table_.try_emplace(entry.ip, entry);
  }

  void add_pending_packet(uint32_t ip, PendingPacket packet) {
    pending_packets_[ip].push_back(packet);
  }

  std::optional<ArpTableEntry> lookup(uint32_t ip) const;

  [[nodiscard]] std::optional<std::vector<PendingPacket>>
  retrieve_pending_packets(uint32_t ip);

private:
  std::unordered_map<uint32_t, ArpTableEntry> arp_table_{};
  std::unordered_map<uint32_t, std::vector<PendingPacket>> pending_packets_{};
};

} // namespace router::arp