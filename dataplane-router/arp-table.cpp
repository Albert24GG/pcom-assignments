#include "arp-table.hpp"

namespace router::arp {

std::optional<ArpTableEntry> ArpTable::lookup(uint32_t ip) const {
  auto it = arp_table_.find(ip);
  if (it != arp_table_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::optional<std::vector<PendingPacket>>
ArpTable::retrieve_pending_packets(uint32_t ip) {
  auto node = pending_packets_.extract(ip);
  if (node) {
    return std::move(node.mapped());
  }
  return std::nullopt;
}

} // namespace router::arp