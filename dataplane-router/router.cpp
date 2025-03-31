#include "router.hpp"
#include "common.hpp"
#include "lib.h"
#include "lib_wrapper.hpp"
#include "logger.hpp"
#include "util.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <spdlog/fmt/bin_to_hex.h>
#include <sys/types.h>

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
  ip_hdr_p->checksum =
      util::hton(checksum(reinterpret_cast<uint16_t *>(ip_hdr_p), IP_HDR_SIZE));
}

// Return a frame containing the ARP request
// If dest_mac is not provided, this means it is a broadcast
std::array<std::byte, ETHER_HDR_SIZE + ARP_HDR_SIZE> generate_arp_frame(
    uint16_t arp_op, uint32_t source_ip, tcb::span<const uint8_t, 6> source_mac,
    uint32_t dest_ip,
    std::optional<tcb::span<const uint8_t, 6>> dest_mac = std::nullopt) {

  std::array<std::byte, ETHER_HDR_SIZE + ARP_HDR_SIZE> frame;
  auto *eth_hdr = reinterpret_cast<struct ether_hdr *>(frame.data());
  auto *arp_hdr =
      reinterpret_cast<struct arp_hdr *>(frame.data() + ETHER_HDR_SIZE);
  *arp_hdr = {.hw_type = util::hton(ARP_HW_TYPE_ETHERNET),
              .proto_type = util::hton(ARP_PROTO_TYPE_IP),
              .hw_len = ARP_HW_LEN,
              .proto_len = ARP_PROTO_LEN,
              .opcode = util::hton(arp_op),
              .shwa = {},
              .sprotoa = source_ip,
              .thwa = {},
              .tprotoa = dest_ip};
  std::copy(source_mac.begin(), source_mac.end(), std::begin(arp_hdr->shwa));
  std::copy(source_mac.begin(), source_mac.end(),
            std::begin(eth_hdr->ethr_shost));

  if (dest_mac) {
    std::copy(dest_mac->begin(), dest_mac->end(),
              std::begin(eth_hdr->ethr_dhost));
    std::copy(dest_mac->begin(), dest_mac->end(), std::begin(arp_hdr->thwa));
  } else {
    std::fill(std::begin(eth_hdr->ethr_dhost), std::end(eth_hdr->ethr_dhost),
              0xff);
    std::fill(std::begin(arp_hdr->thwa), std::end(arp_hdr->thwa), 0x00);
  }

  eth_hdr->ethr_type = util::hton(ETHERTYPE_ARP);
  return frame;
}

} // namespace

std::optional<std::pair<uint32_t, iface_t>>
Router::get_next_hop(uint32_t dest_ip) {
  auto entry = rtable_.lookup(dest_ip);
  if (!entry) {
    return std::nullopt;
  }

  uint32_t next_hop_ip = entry->next_hop;
  iface_t next_hop_iface = entry->interface;

  return std::make_pair(next_hop_ip, next_hop_iface);
}

Router::interface_info Router::get_interface_info(iface_t interface) {
  auto it = interface_ip_map_.find(interface);

  // If the interface info is already known, return it
  if (it != interface_ip_map_.end()) {
    return it->second;
  }

  // Otherwise, get the IP address and MAC address from the interface
  uint32_t ip = get_interface_ip_addr(interface);
  std::array<uint8_t, 6> mac;
  ::get_interface_mac(interface, mac.data());

  interface_info info{ip, mac};
  interface_ip_map_.try_emplace(interface, info);
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
  if (frame.size() < ETHER_HDR_SIZE + ARP_HDR_SIZE) {
    LOG_ERROR("Cannot read ARP header. Packet too small");
    return;
  }

  // Extract the ARP header
  const auto *arp_hdr = reinterpret_cast<const struct arp_hdr *>(
      frame.subspan(ETHER_HDR_SIZE).data());
  uint16_t opcode = util::ntoh(arp_hdr->opcode);

  switch (opcode) {
  case ARP_OPCODE_REQUEST:
    LOG_DEBUG("ARP request");
    handle_arp_request(frame, interface);
    break;
  case ARP_OPCODE_REPLY:
    LOG_DEBUG("ARP reply");
    handle_arp_reply(frame, interface);
    break;
  default:
    LOG_ERROR("Unknown ARP opcode: {}", opcode);
    return;
  }
}
void Router::handle_ip_packet(tcb::span<std::byte> frame, iface_t interface) {
  LOG_DEBUG("Handling IP packet");

  // Check if the packet is too small
  if (frame.size() < ETHER_HDR_SIZE + IP_HDR_SIZE) {
    LOG_ERROR("Cannot read IP header. Packet too small");
    return;
  }

  // Extract the IP header
  auto *ip_hdr_p = reinterpret_cast<struct ip_hdr *>(
      frame.subspan(sizeof(ether_hdr)).data());
  bool for_this_router = is_for_this_router(ip_hdr_p->dest_addr, interface);

  // If TTL reached 1 or 0, we need to drop it
  if (ip_hdr_p->ttl <= 1 && !for_this_router) {
    LOG_DEBUG("TTL reached 0. Dropping packet");
    send_icmp_error(frame, interface, ICMP_TYPE_TIME_EXCEEDED,
                    ICMP_CODE_TTL_EXCEEDED);
    return;
  }

  // Recalculate the checksum
  if (!checksum_valid(ip_hdr_p)) {
    LOG_ERROR("Checksum error. Dropping packet");
    return;
  }

  // Check if the packet is for this router
  if (for_this_router) {
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

  switch (uint8_t proto = ip_hdr->proto) {
  case IP_PROTO_ICMP:
    LOG_DEBUG("ICMP packet");
    handle_icmp_packet(frame, interface);
    break;
  default:
    LOG_ERROR("Unknown IP protocol: {}", proto);
    return;
  }
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
  auto next_hop = get_next_hop(dest_ip);
  if (!next_hop) {
    LOG_ERROR("No matching route found. Dropping packet");
    send_icmp_error(frame, interface, ICMP_TYPE_UNREACH, ICMP_CODE_UNREACH_NET);
    return;
  }
  auto [next_hop_ip, next_hop_iface] = *next_hop;
  LOG_DEBUG("Next hop IP: {:x}, interface: {}", next_hop_ip, next_hop_iface);

  send_frame(frame, next_hop_iface, next_hop_ip, ETHERTYPE_IP);
}

void Router::send_frame(tcb::span<std::byte> frame, iface_t interface,
                        uint32_t dest_ip, uint16_t eth_type) {
  std::array<uint8_t, 6> source_mac = get_interface_mac(interface);
  auto dest_mac_entry = arp_table_.lookup(dest_ip);
  if (!dest_mac_entry) {
    LOG_DEBUG("No matching ARP entry found for IP: {:x}", dest_ip);
    send_arp_request(dest_ip, interface);
    // Cache the packet for later
    arp_table_.add_pending_packet(
        dest_ip,
        {interface, std::vector<std::byte>(frame.begin(), frame.end())});
    return;
  }
  auto dest_mac = dest_mac_entry->mac;

  ether_hdr *eth_hdr = reinterpret_cast<ether_hdr *>(frame.data());
  std::copy(source_mac.begin(), source_mac.end(),
            std::begin(eth_hdr->ethr_shost));
  std::copy(dest_mac.begin(), dest_mac.end(), std::begin(eth_hdr->ethr_dhost));
  eth_hdr->ethr_type = util::hton(eth_type);

  // Send the frame
  LOG_DEBUG("Sending frame to interface {}: {:xpn}", interface,
            spdlog::to_hex(dest_mac));
  send_to_link(frame.size(), reinterpret_cast<char *>(frame.data()), interface);
}

void Router::send_arp_request(uint32_t dest_ip, iface_t interface) {
  auto [source_ip, source_mac] = get_interface_info(interface);

  LOG_DEBUG("Sending ARP request to {:x} on interface {} with MAC {:xpn}",
            dest_ip, interface, spdlog::to_hex(source_mac));

  auto frame =
      generate_arp_frame(ARP_OPCODE_REQUEST, source_ip, source_mac, dest_ip);
  send_to_link(frame.size(), reinterpret_cast<char *>(frame.data()), interface);
}

void Router::handle_arp_reply(tcb::span<std::byte> frame, iface_t interface) {
  LOG_DEBUG("Handling ARP reply");

  // Extract the ARP header
  const auto *arp_hdr = reinterpret_cast<const struct arp_hdr *>(
      frame.subspan(ETHER_HDR_SIZE).data());

  // Store the MAC address in the ARP table
  std::array<uint8_t, 6> sender_mac{};
  std::copy(std::begin(arp_hdr->shwa), std::end(arp_hdr->shwa),
            sender_mac.begin());
  uint32_t sender_ip = arp_hdr->sprotoa;
  arp_table_.add_entry({
      .ip = sender_ip,
      .mac = sender_mac,
  });

  LOG_DEBUG("Stored ARP entry: {:x} -> {:xpn}", sender_ip,
            spdlog::to_hex(sender_mac));

  // Handle any pending packets for this IP address
  auto pending_pkts = arp_table_.retrieve_pending_packets(sender_ip);
  if (!pending_pkts) {
    LOG_DEBUG("No pending packets for IP: {:x}", sender_ip);
    return;
  }

  for (const auto &pending_pkt : *pending_pkts) {
    auto [iface, pkt] = pending_pkt;
    LOG_DEBUG("Sending pending packet to interface {}: {:xpn}", iface,
              spdlog::to_hex(sender_mac));
    send_frame(pkt, iface, sender_ip, ETHERTYPE_IP);
  }
}

void Router::handle_arp_request(tcb::span<std::byte> frame, iface_t interface) {
  LOG_DEBUG("Handling ARP request");

  // Extract the ARP header
  const auto *arp_hdr = reinterpret_cast<const struct arp_hdr *>(
      frame.subspan(ETHER_HDR_SIZE).data());

  // Check if the request is for this router
  if (arp_hdr->tprotoa != get_interface_ip(interface)) {
    LOG_DEBUG("ARP request not for this router. Ignoring");
    return;
  }

  // Send an ARP reply
  std::array<uint8_t, 6> dest_mac{};
  std::copy(std::begin(arp_hdr->shwa), std::end(arp_hdr->shwa),
            dest_mac.begin());
  uint32_t dest_ip = arp_hdr->sprotoa;
  send_arp_reply(dest_ip, interface, dest_mac);
}

void Router::send_arp_reply(uint32_t dest_ip, iface_t interface,
                            const std::array<uint8_t, 6> &dest_mac) {
  auto [source_ip, source_mac] = get_interface_info(interface);

  LOG_DEBUG("Sending ARP reply to {:x} on interface {} with MAC {:xpn}",
            dest_ip, interface, spdlog::to_hex(source_mac));

  auto frame = generate_arp_frame(ARP_OPCODE_REPLY, source_ip, source_mac,
                                  dest_ip, dest_mac);
  send_to_link(frame.size(), reinterpret_cast<char *>(frame.data()), interface);
}

void Router::send_icmp_error(tcb::span<std::byte> frame, iface_t interface,
                             uint8_t type, uint8_t code) {
  LOG_DEBUG("Sending ICMP error: type {}, code {}", type, code);

  std::vector<std::byte> new_frame_buffer{};
  tcb::span<std::byte> icmp_frame{};
  size_t icmp_frame_size = ETHER_HDR_SIZE + 2 * IP_HDR_SIZE + ICMP_HDR_SIZE + 8;

  // Check if the current frame is large enough to modify in place
  if (frame.size() < icmp_frame_size) {
    new_frame_buffer.resize(icmp_frame_size);
    icmp_frame = tcb::span(new_frame_buffer.data(), icmp_frame_size);
    // Copy the original IP header and the first 8 bytes of the payload
    auto old_ip_hdr_payload =
        frame.subspan(ETHER_HDR_SIZE,
                      std::min(frame.size() - ETHER_HDR_SIZE, IP_HDR_SIZE + 8));
    std::copy(old_ip_hdr_payload.begin(), old_ip_hdr_payload.end(),
              icmp_frame.subspan(ETHER_HDR_SIZE).begin());
  } else {
    icmp_frame = frame.subspan(0, icmp_frame_size);
  }

  auto *ip_hdr = reinterpret_cast<struct ip_hdr *>(
      icmp_frame.subspan(ETHER_HDR_SIZE).data());
  auto *icmp_hdr = reinterpret_cast<struct icmp_hdr *>(
      icmp_frame.subspan(ETHER_HDR_SIZE + IP_HDR_SIZE).data());

  uint32_t dest_ip = ip_hdr->source_addr;
  uint32_t source_ip = get_interface_ip(interface);

  // Copy the old IP header and 64 bits of the old payload to the payload of the
  // icmp packet
  {
    auto icmp_payload = icmp_frame.subspan(ETHER_HDR_SIZE, IP_HDR_SIZE + 8);
    std::copy(icmp_payload.begin(), icmp_payload.end(),
              icmp_frame.subspan(ETHER_HDR_SIZE + IP_HDR_SIZE + ICMP_HDR_SIZE)
                  .data());
  }

  ip_hdr->dest_addr = dest_ip;
  ip_hdr->source_addr = source_ip;
  ip_hdr->proto = IP_PROTO_ICMP;
  ip_hdr->ttl = IP_DEFAULT_TTL;
  ip_hdr->tot_len =
      util::hton(static_cast<uint16_t>(icmp_frame.size() - ETHER_HDR_SIZE));
  recompute_checksum(ip_hdr);

  icmp_hdr->mcode = code;
  icmp_hdr->mtype = type;
  icmp_hdr->check = 0;
  std::memset(&icmp_hdr->un_t, 0, sizeof(icmp_hdr->un_t));
  icmp_hdr->check = util::hton(checksum(reinterpret_cast<uint16_t *>(icmp_hdr),
                                        ICMP_HDR_SIZE + 8 + IP_HDR_SIZE));

  send_frame(icmp_frame, interface, dest_ip, ETHERTYPE_IP);
}

void Router::handle_icmp_packet(tcb::span<std::byte> frame, iface_t interface) {
  LOG_DEBUG("Handling ICMP packet");

  // Check if the packet is too small
  if (frame.size() < ETHER_HDR_SIZE + IP_HDR_SIZE + ICMP_HDR_SIZE) {
    LOG_ERROR("Cannot read ICMP header. Packet too small");
    return;
  }

  // Extract the ICMP header
  auto *icmp_hdr = reinterpret_cast<struct icmp_hdr *>(
      frame.subspan(ETHER_HDR_SIZE + IP_HDR_SIZE).data());

  switch (uint8_t type = icmp_hdr->mtype) {
  case ICMP_TYPE_ECHO_REQUEST:
    LOG_DEBUG("ICMP echo request");
    send_icmp_echo_reply(frame, interface);
    break;
  default:
    LOG_ERROR("Received unsupported ICMP type: {}", type);
    return;
  }
}

void Router::send_icmp_echo_reply(tcb::span<std::byte> frame,
                                  iface_t interface) {
  LOG_DEBUG("Sending ICMP echo reply");

  // Extract the ICMP header
  auto *icmp_hdr = reinterpret_cast<struct icmp_hdr *>(
      frame.subspan(ETHER_HDR_SIZE + IP_HDR_SIZE).data());

  // Swap the source and destination IP addresses
  auto *ip_hdr =
      reinterpret_cast<struct ip_hdr *>(frame.subspan(ETHER_HDR_SIZE).data());
  std::swap(ip_hdr->source_addr, ip_hdr->dest_addr);
  ip_hdr->ttl = IP_DEFAULT_TTL;
  recompute_checksum(ip_hdr);

  icmp_hdr->mtype = ICMP_TYPE_ECHO_REPLY;
  icmp_hdr->mcode = ICMP_CODE_ECHO_REPLY;

  // Recalculate the checksum
  recompute_checksum(ip_hdr);
  icmp_hdr->check = 0;
  icmp_hdr->check =
      util::hton(checksum(reinterpret_cast<uint16_t *>(icmp_hdr),
                          frame.size() - ETHER_HDR_SIZE - IP_HDR_SIZE));

  send_frame(frame, interface, ip_hdr->dest_addr, ETHERTYPE_IP);
}

} // namespace router