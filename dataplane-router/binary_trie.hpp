#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

namespace trie {

template <typename Key, typename Value = std::nullptr_t,
          typename = std::enable_if_t<std::is_integral_v<Key> &&
                                      std::is_unsigned_v<Key>>>
class BinaryTrie {
  constexpr static size_t BITS = sizeof(Key) * 8;

public:
  /**
    * @brief Insert a value into the trie at a given path with a specified
    * prefix length.
    * The prefix is represented as a number of bits from the most significant
    * bit to the least significant bit.
    * The path is traversed from the most significant bit to the least

    * @param path The path to insert the value at.
    * @param prefix_len The length of the prefix in bits to be traversed.
    * @param value The value to insert.
*/
  void insert(Key path, size_t prefix_len, Value value) {
    Node *cur = root.get();

    Key mask = 1 << (BITS - 1);
    for (size_t i = 0; i < prefix_len; ++i) {
      size_t index = path & mask ? 1 : 0;
      mask >>= 1;
      if (!cur->children[index]) {
        cur->children[index] = std::make_unique<Node>();
      }
      cur = cur->children[index].get();
    }
    cur->value = std::move(value);
    cur->is_end_of_key = true;
  }

  /**
    * @brief Find the longest prefix match for a given path.
    * The prefix is represented as a number of bits from the most significant
    * bit to the least significant bit.
    * The path is traversed from the most significant bit to the least

    * @param path The path to search for.
    * @return The value associated with the longest prefix match, or
    std::nullopt if no match is found.
*/
  std::optional<Value> longest_prefix_match(Key path) const {
    Node *cur = root.get();
    Node *result = nullptr;

    Key mask = 1 << (BITS - 1);
    for (size_t i = 0; i < BITS; ++i) {
      size_t index = path & mask ? 1 : 0;
      mask >>= 1;
      if (!cur->children[index]) {
        break;
      }
      cur = cur->children[index].get();
      if (cur->is_end_of_key) {
        result = cur;
      }
    }

    if (result) {
      return result->value;
    }
    return std::nullopt;
  }

  /**
    * @brief Erase a value from the trie at a given path with a specified
    * prefix length.
    * The prefix is represented as a number of bits from the most significant
    * bit to the least significant bit.
    * The path is traversed from the most significant bit to the least

    * @param path The path to erase the value from.
    * @param prefix_len The length of the prefix in bits to be traversed.
    * @return true if the value was successfully erased, false otherwise.
*/
  bool erase(Key path, size_t prefix_len) {
    path_buffer.clear();
    path_buffer.reserve(BITS);

    Key mask = 1 << (BITS - 1);

    Node *cur = root.get();
    for (size_t i = 0; i < prefix_len; ++i) {
      size_t index = path & mask ? 1 : 0;
      mask >>= 1;

      if (!cur->children[index]) {
        return false;
      }
      path_buffer.push_back(cur);
      cur = cur->children[index].get();
    }

    if (!cur->is_end_of_key) {
      return false;
    }
    cur->is_end_of_key = false;
    cur->value.reset();

    mask = 1 << (BITS - 1);
    // Iterate in reverse to remove empty nodes
    for (size_t i = 0; i < prefix_len; ++i) {
      size_t index = (path & mask) ? 1 : 0;
      mask >>= 1;
      Node *parent = path_buffer[i];
      Node *child = parent->children[index].get();
      if (child->is_end_of_key || child->children[0] || child->children[1]) {
        break;
      }

      parent->children[index].reset();
    }

    return true;
  }

private:
  struct Node {
    std::array<std::unique_ptr<Node>, 2> children{nullptr, nullptr};
    std::optional<Value> value;
    bool is_end_of_key = false;
  };

  std::unique_ptr<Node> root{std::make_unique<Node>()};
  std::vector<Node *> path_buffer;
};

} // namespace trie