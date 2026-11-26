#include "core/money.hpp"
#include "core/errors.hpp"
#include "core/text.hpp"
#include <charconv>
#include <cstdint>
#include <format>

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

} // namespace

Cents parse_amount(std::string_view raw) {
  const auto s = trim(raw);
  if (s.empty())
    throw ValidationError{"Enter an amount."};

  const auto sep = s.find_first_of(".,");
  const auto whole = s.substr(0, sep);
  const auto fraction =
      sep == std::string_view::npos ? std::string_view{} : s.substr(sep + 1);
  const bool well_formed = !(whole.empty() && fraction.empty()) &&
                           all_digits(whole) && all_digits(fraction) &&
                           whole.size() <= kMaxWholeDigits &&
                           fraction.size() <= kMaxFractionDigits;
  if (!well_formed)
    throw ValidationError{"The amount is not valid."};

  // "5" -> 50 cents, "05" -> 5 cents.
  const Cents frac_cents = to_cents(fraction) * (fraction.size() == 1 ? 10 : 1);
  const Cents total = to_cents(whole) * 100 + frac_cents;
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
