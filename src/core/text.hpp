#pragma once

#include <algorithm>
#include <string_view>

namespace fin {

/// Trims ASCII whitespace from both ends (independent of the locale).
[[nodiscard]] constexpr std::string_view trim(std::string_view s) noexcept {
  constexpr std::string_view kSpace = " \t\n\r\f\v";
  const auto first = s.find_first_not_of(kSpace);
  if (first == std::string_view::npos)
    return {};
  return s.substr(first, s.find_last_not_of(kSpace) - first + 1);
}

[[nodiscard]] constexpr bool all_digits(std::string_view s) noexcept {
  return std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace fin
