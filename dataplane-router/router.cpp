#include "router.hpp"
#include "lib_wrapper.hpp"
#include "logger.hpp"
#include "util.hpp"
#include <cstdint>
#include <optional>
#include <spdlog/fmt/bin_to_hex.h>

namespace router {

namespace {

bool checksum_valid(struct ip_hdr *ip_hdr_p) {
  // We can avoid setting the checksum field to 0. Due to the way the checksum
  // is calculated, if the current checksum has been computed with the field
  // set to 0, this means recalculating it will yield 0 in case of no errors.
  // This is because the checksum is a 1's complement sum of all 16-bit words
  return checksum(reinterpret_cast<uint16_t *>(ip_hdr_p),
                  sizeof(struct ip_hdr)) == 0;
}

void recompute_checksum(struct ip_hdr *ip_hdr_p) {
  ip_hdr_p->checksum = 0;
  ip_hdr_p->checksum = util::hton(
      checksum(reinterpret_cast<uint16_t *>(ip_hdr_p), sizeof(struct ip_hdr)));
}
} // namespace

Router::interface_info Router::get_interface_info(iface_t interface) {
  auto it = interface_ip_map.find(interface);

  // If the interface info is already known, return it
  if (it != interface_ip_map.end()) {
    return it->second;
  }

  // Otherwise, get the IP address and MAC address from the interface
  uint32_t ip = get_interface_ip_addr(interface);
  std::array<uint8_t, 6> mac;
  ::get_interface_mac(interface, mac.data());

  interface_info info{ip, mac};
  interface_ip_map.try_emplace(interface, info);
  // LOG_DEBUG("Inserted interface-info pair: {} -> {}", interface, info);
  LOG_DEBUG("Inserted interface-info pair: {} -> {{ ip: {:x}, mac: {:xpn} }}",
            interface, info.ip, spdlog::to_hex(info.mac));

  return info;
}

void Router::handle_frame(tcb::span<std::byte> frame, iface_t interface) {
  // Check if the packet is too small
  if (frame.size() < sizeof(ether_hdr)) {
    LOG_ERROR("Cannot read ethernet header. Packet too small");
    return;
  }

  // Extract the ethernet header
  const ether_hdr *eth_hdr = reinterpret_cast<const ether_hdr *>(frame.data());
  uint16_t eth_type = util::ntoh(eth_hdr->ethr_type);

  switch (eth_type) {
  case ETHERTYPE_ARP:
    handle_arp_packet(frame, interface);
    break;
  case ETHERTYPE_IP:
    handle_ip_packet(frame, interface);
    break;
  default:
    LOG_ERROR("Unknown ethernet type: {}", eth_type);
    return;
  }
}

void Router::handle_arp_packet(tcb::span<std::byte> frame, iface_t interface) {
  LOG_DEBUG("Handling ARP packet");

  // Check if the packet is too small
  if (frame.size() < sizeof(arp_hdr)) {
    LOG_ERROR("Cannot read ARP header. Packet too small");
    return;
  }
}
void Router::handle_ip_packet(tcb::span<std::byte> frame, iface_t interface) {
  LOG_DEBUG("Handling IP packet");

  // Check if the packet is too small
  if (frame.size() < sizeof(ether_hdr) + sizeof(ip_hdr)) {
    LOG_ERROR("Cannot read IP header. Packet too small");
    return;
  }

  // Extract the IP header
  auto *ip_hdr_p = reinterpret_cast<struct ip_hdr *>(
      frame.subspan(sizeof(ether_hdr)).data());

  // If TTL reached 1 or 0, we need to drop it
  if (ip_hdr_p->ttl <= 1) {
    LOG_DEBUG("TTL reached 0 or 1. Dropping packet");
    // TODO: Send ICMP TTL exceeded message
    return;
  }

  // Recalculate the checksum
  if (!checksum_valid(ip_hdr_p)) {
    LOG_ERROR("Checksum error. Dropping packet");
    return;
  }

  // Check if the packet is for this router
  if (is_for_this_router(ip_hdr_p->dest_addr, interface)) {
    handle_local_ip_packet(frame, interface);
    return;
  }

  // Decrement the TTL
  --ip_hdr_p->ttl;

  // Recalculate the checksum
  recompute_checksum(ip_hdr_p);

  handle_forward_ip_packet(frame, interface);
}

void Router::handle_local_ip_packet(tcb::span<std::byte> frame,
                                    iface_t interface) {
  LOG_DEBUG("Handling local IP packet");

  // The frame size has already been checked in handle_ip_packet
  // Extract the IP header
  const auto *ip_hdr = reinterpret_cast<const struct ip_hdr *>(
      frame.subspan(sizeof(ether_hdr)).data());
}

void Router::handle_forward_ip_packet(tcb::span<std::byte> frame,
                                      iface_t interface) {
  LOG_DEBUG("Handling forward IP packet");

  // The frame size has already been checked in handle_ip_packet
  // Extract the IP header
  const auto *ip_hdr = reinterpret_cast<const struct ip_hdr *>(
      frame.subspan(sizeof(ether_hdr)).data());

  uint32_t dest_ip = ip_hdr->dest_addr;

  LOG_DEBUG("Destination IP: {:x}", dest_ip);
  // TODO: Use trie to find the next hop
  auto lookup_next_hop = [](uint32_t dest_ip,
                            const std::vector<route_table_entry> &rtable)
      -> std::optional<std::pair<uint32_t, uint32_t>> {
    std::optional<std::pair<uint32_t, uint32_t>> next_hop;
    uint32_t best_mask = 0;
    for (const auto &entry : rtable) {
      if ((dest_ip & entry.mask) == entry.prefix && entry.mask > best_mask) {
        best_mask = entry.mask;
        next_hop = std::make_pair(entry.next_hop, entry.interface);
      }
    }
    return next_hop;
  };

  auto next_hop = lookup_next_hop(dest_ip, route_table);
  if (!next_hop) {
    LOG_ERROR("No matching route found. Dropping packet");
    // TODO: Send ICMP destination unreachable message
    return;
  }
  auto [next_hop_ip, next_hop_iface] = *next_hop;
  LOG_DEBUG("Next hop IP: {:x}, interface: {}", next_hop_ip, next_hop_iface);

  send_frame(frame, next_hop_iface, next_hop_ip, ETHERTYPE_IP);
}

void Router::send_frame(tcb::span<std::byte> frame, iface_t interface,
                        uint32_t dest_ip, uint16_t eth_type) {

  std::array<uint8_t, 6> source_mac = get_interface_mac(interface);
  // TODO: Use dynamic arp
  std::array<uint8_t, 6> dest_mac = arp_table[dest_ip];

  ether_hdr *eth_hdr = reinterpret_cast<ether_hdr *>(frame.data());
  std::copy(source_mac.begin(), source_mac.end(), eth_hdr->ethr_shost);
  std::copy(dest_mac.begin(), dest_mac.end(), eth_hdr->ethr_dhost);
  eth_hdr->ethr_type = util::ntoh(eth_type);

  // Send the frame
  LOG_DEBUG("Sending frame to interface {}: {:x}", interface,
            spdlog::to_hex(dest_mac));
  send_to_link(frame.size(), reinterpret_cast<char *>(frame.data()), interface);
}

} // namespace router