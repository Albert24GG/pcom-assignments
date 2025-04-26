#include "token_pattern.hpp"
#include "util.hpp"
#include <queue>
#include <stdexcept>

bool TokenPattern::is_valid_pattern() const {
  if (tokens_.empty() || !is_valid_token(tokens_[0])) {
    return false;
  }

  bool prev_is_wildcard = is_wildcard(tokens_[0]);

  for (size_t i = 1; i < tokens_.size(); ++i) {
    bool cur_is_wildcard{};

    // Check that the current token is valid and that there are no 2 consecutive
    // wildcards
    if (!is_valid_token(tokens_[i]) ||
        ((cur_is_wildcard = is_wildcard(tokens_[i])) && prev_is_wildcard)) {
      return false;
    }

    prev_is_wildcard = cur_is_wildcard;
  }

  return true;
}

TokenPattern TokenPattern::from_string(std::string_view str) {
  if (str.empty()) {
    throw std::invalid_argument("Input string is empty");
  }

  TokenPattern token_pat{};

  size_t offset = 0;

  while (offset < str.size()) {
    size_t pos = str.find_first_of(separator_, offset);
    std::string token;

    if (pos == offset) {
      ++offset;
      continue;
    }

    if (pos == std::string_view::npos) {
      token = str.substr(offset);
      offset = str.size();
    } else if (pos > offset) {
      token = str.substr(offset, pos - offset);
      offset = pos + 1;
    }

    if (is_valid_token(token)) {
      token_pat.tokens_.push_back(token);
    } else {
      throw std::invalid_argument("Invalid token: " + std::string(token));
    }
  }

  if (!token_pat.is_valid_pattern()) {
    throw std::invalid_argument("Invalid token pattern");
  }

  return token_pat;
}

bool TokenPattern::matches(const TokenPattern &other) const {
  if (other.has_wildcard()) {
    throw std::invalid_argument(
        "The TokenPattern to match against contains wildcards");
  }

  std::queue<std::pair<size_t, size_t>> positions{};
  positions.push({0, 0});

  while (!positions.empty()) {
    auto [this_index, other_index] = positions.front();
    positions.pop();

    if (this_index == tokens_.size() && other_index == other.tokens_.size()) {
      return true;
    }

    if (this_index >= tokens_.size() || other_index >= other.tokens_.size()) {
      continue;
    }

    if (tokens_[this_index] == "*") {
      ++this_index;

      if (this_index == tokens_.size()) {
        return true;
      }

      auto low_limit = other.tokens_.rend() - other_index;
      auto it =
          std::find(other.tokens_.rbegin(), low_limit, tokens_[this_index]);
      while (it != low_limit) {
        positions.push(
            {this_index + 1, std::distance(it, other.tokens_.rend())});
        ++it;
        it = std::find(it, low_limit, tokens_[this_index]);
      }

    } else if (tokens_[this_index] == "+" ||
               tokens_[this_index] == other.tokens_[other_index]) {
      positions.push({this_index + 1, other_index + 1});
    }
  }

  return false;
}

std::size_t TokenPattern::hashValue() const {
  std::size_t seed = 0;

  for (const auto &token : tokens_) {
    hash_combine(seed, token);
  }

  return seed;
}
