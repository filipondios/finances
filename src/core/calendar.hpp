#pragma once

#include "core/types.hpp"
#include <optional>
#include <string>
#include <string_view>

namespace fin {

/// "2026-11" -> year_month. Strict: four digits, dash, two digits.
[[nodiscard]] std::optional<YearMonth>
parse_year_month(std::string_view s) noexcept;

/// "2026-11-26" -> year_month_day, rejecting non-existent dates.
[[nodiscard]] std::optional<Date> parse_date(std::string_view s) noexcept;

[[nodiscard]] std::string format_date(Date d);             // "2026-11-26"
[[nodiscard]] std::string format_month_key(YearMonth m);   // "2026-11"
[[nodiscard]] std::string format_month_label(YearMonth m); // "Nov 2026"

[[nodiscard]] constexpr YearMonth month_of(Date d) noexcept {
  return {d.year(), d.month()};
}

/// Months the app can parse and format (years 0000-9999).
[[nodiscard]] constexpr bool in_supported_range(YearMonth m) noexcept {
  return m.year() >= std::chrono::year{0} && m.year() <= std::chrono::year{9999};
}

/// Lexicographic bounds of the month for comparisons against the `date`
/// column ("YYYY-MM-DD"): every valid day of the month falls in [first_day_key,
/// last_day_key].
[[nodiscard]] std::string first_day_key(YearMonth m); // "2026-11-01"
[[nodiscard]] std::string last_day_key(YearMonth m);  // "2026-11-31"

/// Today's date in the local time zone (UTC if no time zone database is available).
[[nodiscard]] Date today();

} // namespace fin
