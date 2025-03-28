#include "lib.h"

#include <arpa/inet.h>
#include <asm/byteorder.h>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

int interfaces[ROUTER_NUM_INTERFACES];

int get_sock(const char *if_name) {
  int res;
  int s = socket(AF_PACKET, SOCK_RAW, 768);
  DIE(s == -1, "socket");

  struct ifreq intf;
  strcpy(intf.ifr_name, if_name);
  res = ioctl(s, SIOCGIFINDEX, &intf);
  DIE(res, "ioctl SIOCGIFINDEX");

  struct sockaddr_ll addr;
  memset(&addr, 0x00, sizeof(addr));
  addr.sll_family = AF_PACKET;
  addr.sll_ifindex = intf.ifr_ifindex;

  res = bind(s, (struct sockaddr *)&addr, sizeof(addr));
  DIE(res == -1, "bind");
  return s;
}

int send_to_link(size_t length, char *frame_data, size_t intidx) {
  /*
   * Note that "buffer" should be at least the MTU size of the
   * interface, eg 1500 bytes
   */
  int ret;
  ret = write(interfaces[intidx], frame_data, length);
  DIE(ret == -1, "write");
  return ret;
}

ssize_t receive_from_link(int intidx, char *frame_data) {
  ssize_t ret;
  ret = read(interfaces[intidx], frame_data, MAX_PACKET_LEN);
  return ret;
}

int socket_receive_message(int sockfd, char *frame_data, size_t *len) {
  /*
   * Note that "buffer" should be at least the MTU size of the
   * interface, eg 1500 bytes
   * */
  int ret = read(sockfd, frame_data, MAX_PACKET_LEN);
  DIE(ret < 0, "read");
  *len = ret;
  return 0;
}

size_t recv_from_any_link(char *frame_data, size_t *length) {
  int res;
  fd_set set;

  FD_ZERO(&set);
  while (1) {
    for (int i = 0; i < ROUTER_NUM_INTERFACES; i++) {
      FD_SET(interfaces[i], &set);
    }

    res = select(interfaces[ROUTER_NUM_INTERFACES - 1] + 1, &set, NULL, NULL,
                 NULL);
    DIE(res == -1, "select");

    for (int i = 0; i < ROUTER_NUM_INTERFACES; i++) {
      if (FD_ISSET(interfaces[i], &set)) {
        ssize_t ret = receive_from_link(i, frame_data);
        DIE(ret < 0, "receive_from_link");
        *length = ret;
        return i;
      }
    }
  }

  return -1;
}

char *get_interface_ip(int interface) {
  struct ifreq ifr;
  int ret;
  if (interface == 0)
    sprintf(ifr.ifr_name, "rr-0-1");
  else {
    sprintf(ifr.ifr_name, "r-%u", interface - 1);
  }
  ret = ioctl(interfaces[interface], SIOCGIFADDR, &ifr);
  DIE(ret == -1, "ioctl SIOCGIFADDR");
  return inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr);
}

void get_interface_mac(size_t interface, uint8_t *mac) {
  struct ifreq ifr;
  int ret;
  if (interface == 0)
    sprintf(ifr.ifr_name, "rr-0-1");
  else {
    sprintf(ifr.ifr_name, "r-%lu", interface - 1);
  }
  ret = ioctl(interfaces[interface], SIOCGIFHWADDR, &ifr);
  DIE(ret == -1, "ioctl SIOCGIFHWADDR");
  memcpy(mac, ifr.ifr_addr.sa_data, 6);
}

static int hex2num(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;

  return -1;
}

int hex2byte(const char *hex) {
  int a, b;
  a = hex2num(*hex++);
  if (a < 0)
    return -1;
  b = hex2num(*hex++);
  if (b < 0)
    return -1;

  return (a << 4) | b;
}

int hwaddr_aton(const char *txt, uint8_t *addr) {
  int i;
  for (i = 0; i < 6; i++) {
    int a, b;
    a = hex2num(*txt++);
    if (a < 0)
      return -1;
    b = hex2num(*txt++);
    if (b < 0)
      return -1;
    *addr++ = (a << 4) | b;
    if (i < 5 && *txt++ != ':')
      return -1;
  }
  return 0;
}

void init(char *argv[], int argc) {
  for (int i = 0; i < argc; ++i) {
    printf("Setting up interface: %s\n", argv[i]);
    interfaces[i] = get_sock(argv[i]);
  }
}

uint16_t checksum(uint16_t *data, size_t length) {
  unsigned long checksum = 0;
  uint16_t extra_byte;
  while (length > 1) {
    checksum += ntohs(*data++);
    length -= 2;
  }
  if (length) {
    *(uint8_t *)&extra_byte = *(uint8_t *)data;
    checksum += extra_byte;
  }

  checksum = (checksum >> 16) + (checksum & 0xffff);
  checksum += (checksum >> 16);
  return (uint16_t)(~checksum);
}

std::vector<route_table_entry>
parse_route_table(const std::filesystem::path &filename) {
  std::ifstream file(filename);

  if (!file.is_open()) {
    throw std::runtime_error("Unable to open file: " + filename.string());
  }
  std::vector<route_table_entry> route_table;

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    route_table_entry entry{};

    std::string prefix_str;
    std::string next_hop_str;
    std::string mask_str;
    if (!(iss >> prefix_str >> next_hop_str >> mask_str >> entry.interface)) {
      throw std::runtime_error("Invalid line format: " + line);
    }

    if (inet_pton(AF_INET, prefix_str.c_str(), &entry.prefix) != 1) {
      throw std::runtime_error("Invalid prefix: " + prefix_str);
    }

    if (inet_pton(AF_INET, next_hop_str.c_str(), &entry.next_hop) != 1) {
      throw std::runtime_error("Invalid next hop: " + next_hop_str);
    }

    if (inet_pton(AF_INET, mask_str.c_str(), &entry.mask) != 1) {
      throw std::runtime_error("Invalid mask: " + mask_str);
    }

    route_table.push_back(entry);
  }

  return route_table;
}

std::array<uint8_t, 6> parse_mac_address(std::string_view mac_str) {
  std::array<uint8_t, 6> mac = {0};
  if (mac_str.length() != 17) {
    throw std::runtime_error(
        "Invalid MAC address format: " + std::string(mac_str) +
        " (expected 17 characters)");
  }

  for (auto i = 1; i <= 5; ++i) {
    if (mac_str[(i * 3) - 1] != ':') {
      throw std::runtime_error(
          "Invalid MAC address format: " + std::string(mac_str) +
          "\nExpected format is XX:XX:XX:XX:XX:XX");
    }
  }

  for (auto i = 0; i < 6; ++i) {
    auto byte_str = mac_str.substr(i * 3, 2);
    auto [ptr, ec] =
        std::from_chars(byte_str.begin(), byte_str.end(), mac[i], 16);
    if (ec != std::errc()) {
      throw std::runtime_error(
          "Invalid MAC address format: " + std::string(mac_str) +
          "\nExpected hexadecimal byte");
    }
  }

  return mac;
}

std::vector<arp_table_entry>
parse_arp_table(const std::filesystem::path &filename) {
  std::vector<arp_table_entry> arp_table;
  std::ifstream file(filename);

  if (!file.is_open()) {
    throw std::runtime_error("Unable to open file: " + filename.string());
  }

  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    arp_table_entry entry{};

    std::string ip_str;
    std::string mac_str;
    if (!(iss >> ip_str >> mac_str)) {
      throw std::runtime_error("Invalid line format: " + line);
    }

    if (inet_pton(AF_INET, ip_str.c_str(), &entry.ip) != 1) {
      throw std::runtime_error("Invalid IP address: " + ip_str);
    }

    entry.mac = parse_mac_address(mac_str);
    arp_table.push_back(entry);
  }

  return arp_table;
}