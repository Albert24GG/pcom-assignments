#pragma once

#include <cstdint>
extern "C" {
#include "lib.h"
#include "protocols.h"
}

constexpr uint16_t ETHERTYPE_ARP = 0x0806;
constexpr uint16_t ETHERTYPE_IP = 0x0800;