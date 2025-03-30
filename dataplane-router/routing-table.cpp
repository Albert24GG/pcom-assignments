#include "routing-table.hpp"

namespace router {
void RoutingTable::add_entries(tcb::span<const RoutingTableEntry> entries) {
  for (const auto &entry : entries) {
    add_entry(entry);
  }
}

void RoutingTable::add_entry(RoutingTableEntry entry) {
  int prefix_len = util::countl_one(util::ntoh(entry.mask));
  uint32_t path = util::ntoh(entry.prefix);
  route_trie_.insert(path, prefix_len, entry);
}

} // namespace router