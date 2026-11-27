#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
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

/// Number of code points of `s`, or nullopt if it is not well-formed UTF-8 or
/// contains control characters (C0, DEL, C1). Rejects overlong forms,
/// surrogates and values above U+10FFFF.
[[nodiscard]] constexpr std::optional<std::size_t>
text_length(std::string_view s) noexcept {
  std::size_t count = 0;
  for (std::size_t i = 0; i < s.size(); ++count) {
    const auto lead = static_cast<unsigned char>(s[i]);
    std::size_t len = 0;
    char32_t cp = 0;
    if (lead < 0x80) {
      len = 1;
      cp = lead;
    } else if (lead >= 0xC2 && lead <= 0xDF) {
      len = 2;
      cp = lead & 0x1Fu;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      len = 3;
      cp = lead & 0x0Fu;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      len = 4;
      cp = lead & 0x07u;
    } else {
      return std::nullopt;
    }
    if (i + len > s.size())
      return std::nullopt;
    for (std::size_t k = 1; k < len; ++k) {
      const auto next = static_cast<unsigned char>(s[i + k]);
      if ((next & 0xC0u) != 0x80u)
        return std::nullopt;
      cp = (cp << 6) | (next & 0x3Fu);
    }
    const bool overlong = (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000);
    const bool surrogate = cp >= 0xD800 && cp <= 0xDFFF;
    const bool control = cp < 0x20 || (cp >= 0x7F && cp < 0xA0);
    if (overlong || surrogate || cp > 0x10FFFF || control)
      return std::nullopt;
    i += len;
  }
  return count;
}

[[nodiscard]] constexpr bool all_digits(std::string_view s) noexcept {
  return std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
}

} // namespace fin
