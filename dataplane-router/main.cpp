#include "logger.hpp"
#include "router.hpp"
#include "span.hpp"
#include <vector>

static constexpr size_t MAX_ROUTING_TABLE_SIZE = 1e5;

int main(int argc, char *argv[]) {
  // c buf[MAX_PACKET_LEN];
  std::array<std::byte, MAX_PACKET_LEN> buf;

  const char *rtable_path = argv[1];

  // Do not modify this line
  init(argv + 2, argc - 2);

  // Initialize the logger
  logger::set_level(logger::Level::debug);
  logger::init();
  LOG_INFO("Router started");

  // Read the routing table
  std::vector<struct route_table_entry> rtable(MAX_ROUTING_TABLE_SIZE);

  int rtable_size = read_rtable(rtable_path, rtable.data());
  rtable.resize(rtable_size);
  LOG_INFO("Routing table read with {} entries", rtable_size);

#ifdef DEBUG
  uint32_t prefix = rtable[0].prefix;
  uint32_t mask = rtable[0].mask;
  uint32_t next_hop = rtable[0].next_hop;
  int interface = rtable[0].interface;
  LOG_DEBUG("First route entry prefix: {:x} mask: {:x} next_hop: {:x} "
            "interface: {}",
            prefix, mask, next_hop, interface);
#endif

  // Initialize the router
  router::Router router{};
  router.add_rtable_entries(rtable);

  while (true) {
    size_t interface;
    size_t len;

    interface = recv_from_any_link(reinterpret_cast<char *>(buf.data()), &len);
    DIE(interface < 0, "recv_from_any_links");

    auto frame = tcb::span<std::byte>(buf.data(), len);
    LOG_DEBUG("Received frame of size {} on interface {}", len, interface);

    router.handle_frame(frame, interface);

    // TODO: Implement the router forwarding logic

    /* Note that packets received are in network order,
                any header field which has more than 1 byte will need to be
       conerted to host order. For example, ntohs(eth_hdr->ether_type). The
       oposite is needed when sending a packet on the link, */
  }
}
