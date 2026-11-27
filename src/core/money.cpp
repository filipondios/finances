#include "core/money.hpp"
#include "core/errors.hpp"
#include "core/text.hpp"
#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <string>

namespace fin {
namespace {

constexpr std::size_t kMaxWholeDigits = 12;
constexpr std::size_t kMaxFractionDigits = 2;

Cents to_cents(std::string_view digits) {
  Cents value = 0;
  if (!digits.empty())
    std::from_chars(digits.data(), digits.data() + digits.size(), value);
  return value;
}

/// "1.234" with sep '.' -> "1234"; nullopt unless the groups are well formed.
std::optional<std::string> ungroup(std::string_view text, char sep) {
  std::string digits;
  for (bool first = true;; first = false) {
    const auto cut = text.find(sep);
    const auto group = text.substr(0, cut);
    const bool size_ok =
        first ? (group.size() >= 1 && group.size() <= 3) : group.size() == 3;
    if (!size_ok || !all_digits(group))
      return std::nullopt;
    digits += group;
    if (cut == std::string_view::npos)
      return digits;
    text.remove_prefix(cut + 1);
  }
}

struct AmountParts {
  std::string whole; ///< digits only
  std::string_view fraction;
};

std::optional<AmountParts> split_amount(std::string_view s) {
  const auto last = s.find_last_of(".,");
  if (last == std::string_view::npos)
    return AmountParts{std::string{s}, {}};

  const char sep = s[last];
  const char other = sep == '.' ? ',' : '.';
  const auto head = s.substr(0, last);
  const auto tail = s.substr(last + 1);

  if (head.find_first_of(".,") == std::string_view::npos) // "12,5"
    return AmountParts{std::string{head}, tail};
  if (head.find(sep) == std::string_view::npos) { // "1.234,56": `other` groups
    auto whole = ungroup(head, other);
    return whole ? std::optional{AmountParts{std::move(*whole), tail}}
                 : std::nullopt;
  }
  if (head.find(other) != std::string_view::npos) // "1.234,567,89"
    return std::nullopt;
  auto whole = ungroup(s, sep); // "1.234.567": only thousands groups
  return whole ? std::optional{AmountParts{std::move(*whole), {}}}
               : std::nullopt;
}

} // namespace

Cents parse_amount(std::string_view raw) {
  const auto s = trim(raw);
  if (s.empty())
    throw ValidationError{"Enter an amount."};

  const auto parts = split_amount(s);
  const bool well_formed =
      parts && !(parts->whole.empty() && parts->fraction.empty()) &&
      all_digits(parts->whole) && all_digits(parts->fraction) &&
      parts->whole.size() <= kMaxWholeDigits &&
      parts->fraction.size() <= kMaxFractionDigits;
  if (!well_formed)
    throw ValidationError{"The amount is not valid."};

  // "5" -> 50 cents, "05" -> 5 cents.
  const Cents frac_cents =
      to_cents(parts->fraction) * (parts->fraction.size() == 1 ? 10 : 1);
  const Cents total = to_cents(parts->whole) * 100 + frac_cents;
  if (total <= 0)
    throw ValidationError{"The amount is not valid."};
  return total;
}

std::string format_money(Cents cents) {
  const bool negative = cents < 0;
  // Negation in unsigned arithmetic: safe even for INT64_MIN.
  const auto abs = negative ? 0 - static_cast<std::uint64_t>(cents)
                            : static_cast<std::uint64_t>(cents);
  std::string whole = std::to_string(abs / 100);
  for (auto i = whole.size(); i > 3; i -= 3)
    whole.insert(i - 3, 1, '.');
  return std::format("{}{},{:02} €", negative ? "-" : "", whole, abs % 100);
}

} // namespace fin
