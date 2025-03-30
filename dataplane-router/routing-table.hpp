#pragma once

#include "binary_trie.hpp"
#include "lib_wrapper.hpp"
#include "span.hpp"
#include "util.hpp"
#include <cstdint>
#include <optional>

namespace router {

class RoutingTable {
public:
  using RoutingTableEntry = route_table_entry;

  void add_entries(tcb::span<const RoutingTableEntry> entries);

  void add_entry(RoutingTableEntry entry);

  [[nodiscard]] std::optional<RoutingTableEntry>
  lookup(uint32_t dest_ip) const {
    return route_trie_.longest_prefix_match(util::hton(dest_ip));
  }

private:
  trie::BinaryTrie<uint32_t, RoutingTableEntry> route_trie_{};
};

} // namespace router