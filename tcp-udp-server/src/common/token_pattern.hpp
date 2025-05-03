#pragma once

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

class TokenPattern {

public:
  /**
   * @brief Build a TokenPattern from a string
   *
   * The string must represent a list of tokens separated by '/'
   * The valid tokens are
   * - non-empty string
   * - '*' wildcard - matches 1 or more string tokens (greedy)
   * - '+' wildcard - matches 1 string tokens
   *
   * @param str The string to parse
   * @return A TokenPattern object
   *
   * @throws std::invalid_argument if the string is empty or contains invalid
   * tokens
   *
   */
  [[nodiscard]] static TokenPattern from_string(std::string_view str);

  /**
   * @brief Check if the TokenPattern matches another TokenPattern
   *
   * The TokenPattern matched agains must only contain string tokens
   *
   * The matching rules are:
   * - '*' matches 1 or more string tokens (greedy)
   * - '+' matches 1 string token
   * - string matches the same string
   *
   * @param other The TokenPattern to match against
   * @return true if the TokenPattern matches the other TokenPattern
   *
   * @throws std::invalid_argument if the other TokenPattern contains wildcards
   */
  [[nodiscard]] bool matches(const TokenPattern &other) const;

  /**
   * @brief Compute the hash value of the TokenPattern
   *
   * @return The hash value of the TokenPattern
   */
  std::size_t hashValue() const;

  friend bool operator==(const TokenPattern &lhs, const TokenPattern &rhs) {
    return lhs.tokens_ == rhs.tokens_;
  }

private:
  static constexpr bool is_valid_token(std::string_view token) {
    return token.size() > 0;
  }

  static constexpr bool is_wildcard(std::string_view token) {
    return token == "*" || token == "+";
  }

  bool has_wildcard() const {
    return std::any_of(tokens_.begin(), tokens_.end(), is_wildcard);
  }

  bool is_valid_pattern() const;

  std::vector<std::string> tokens_{};
  static constexpr char separator_{'/'};
};

// Custom specialization of std::hash for TokenPattern
template <> struct std::hash<TokenPattern> {
  std::size_t operator()(const TokenPattern &token_pattern) const {
    return token_pattern.hashValue();
  }
};
