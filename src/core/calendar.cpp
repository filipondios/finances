#include "core/calendar.hpp"
#include "core/text.hpp"
#include <charconv>
#include <exception>
#include <format>

namespace fin {
namespace {
using namespace std::chrono;

constexpr std::array<std::string_view, 12> kMonthNames{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/// Parses an unsigned integer made only of digits.
std::optional<int> to_int(std::string_view s) noexcept {
  int value = 0;
  if (s.empty() || !all_digits(s))
    return std::nullopt;
  const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || end != s.data() + s.size())
    return std::nullopt;
  return value;
}
} // namespace

std::optional<YearMonth> parse_year_month(std::string_view s) noexcept {
  if (s.size() != 7 || s[4] != '-')
    return std::nullopt;
  const auto y = to_int(s.substr(0, 4));
  const auto m = to_int(s.substr(5, 2));
  if (!y || !m)
    return std::nullopt;
  const YearMonth ym{year{*y}, month{static_cast<unsigned>(*m)}};
  return ym.ok() ? std::optional{ym} : std::nullopt;
}

std::optional<Date> parse_date(std::string_view s) noexcept {
  if (s.size() != 10 || s[4] != '-' || s[7] != '-')
    return std::nullopt;
  const auto y = to_int(s.substr(0, 4));
  const auto m = to_int(s.substr(5, 2));
  const auto d = to_int(s.substr(8, 2));
  if (!y || !m || !d)
    return std::nullopt;
  const Date ymd{year{*y}, month{static_cast<unsigned>(*m)},
                 day{static_cast<unsigned>(*d)}};
  return ymd.ok() ? std::optional{ymd} : std::nullopt;
}

std::string format_date(Date d) {
  return std::format("{:04}-{:02}-{:02}", static_cast<int>(d.year()),
                     static_cast<unsigned>(d.month()),
                     static_cast<unsigned>(d.day()));
}

std::string format_month_key(YearMonth m) {
  return std::format("{:04}-{:02}", static_cast<int>(m.year()),
                     static_cast<unsigned>(m.month()));
}

std::string format_month_label(YearMonth m) {
  return std::format("{} {:04}", kMonthNames[unsigned{m.month()} - 1],
                     static_cast<int>(m.year()));
}

std::string first_day_key(YearMonth m) { return format_month_key(m) + "-01"; }
std::string last_day_key(YearMonth m) { return format_month_key(m) + "-31"; }

Date today() {
  const auto now = system_clock::now();
  try {
    return Date{floor<days>(current_zone()->to_local(now))};
  } catch (const std::exception &) {
    return Date{floor<days>(now)};
  }
}

} // namespace fin
