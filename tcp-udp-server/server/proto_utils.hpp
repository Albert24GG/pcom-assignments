#pragma once

#include <algorithm>
#include <variant>

template <typename PayloadVariant, auto... Idx>
constexpr size_t max_serialized_size_impl(std::index_sequence<Idx...>) {
  return std::max(
      {std::variant_alternative_t<Idx,
                                  PayloadVariant>::MAX_SERIALIZED_SIZE...});
}

// Helper function to get the maximum serialized size of a payload variant
template <typename PayloadVariant> constexpr size_t max_serialized_size() {
  return max_serialized_size_impl<PayloadVariant>(
      std::make_index_sequence<std::variant_size_v<PayloadVariant>>{});
}

template <typename PayloadVariant, auto... Idx>
constexpr size_t min_serialized_size_impl(std::index_sequence<Idx...>) {
  return std::min(
      {std::variant_alternative_t<Idx,
                                  PayloadVariant>::MIN_SERIALIZED_SIZE...});
}

// Helper function to get the minimum serialized size of a payload variant
template <typename PayloadVariant> constexpr size_t min_serialized_size() {
  return min_serialized_size_impl<PayloadVariant>(
      std::make_index_sequence<std::variant_size_v<PayloadVariant>>{});
}
