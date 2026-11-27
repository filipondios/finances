#include "core/calendar.hpp"
#include "support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

using namespace fin;
using fin::test::ym;
using fin::test::ymd;

TEST_CASE("parse_year_month", "[calendar]") {
  CHECK(parse_year_month("2026-11") == ym(2026, 11));
  CHECK(parse_year_month("0000-01") == ym(0, 1));
  const auto bad = GENERATE(as<std::string_view>{}, "", "2026-13", "2026-00",
                            "2026-1", "26-11", "2026/11", "+026-11", "2026-1a",
                            " 2026-11", "2026-11 ", "10000-01");
  CHECK_FALSE(parse_year_month(bad).has_value());
}

TEST_CASE("parse_date accepts real dates only", "[calendar]") {
  CHECK(parse_date("2026-11-26") == ymd(2026, 11, 26));
  CHECK(parse_date("2024-02-29") == ymd(2024, 2, 29)); // leap year
  const auto bad = GENERATE(as<std::string_view>{}, "", "2026-02-29",
                            "2026-04-31", "2026-11-00", "2026-13-01",
                            "2026-1-01", "2026-11-1", "2026/11/26",
                            "2026-11-26 ", "20261126");
  CHECK_FALSE(parse_date(bad).has_value());
}

TEST_CASE("formatting round-trips", "[calendar]") {
  CHECK(format_date(ymd(2026, 1, 5)) == "2026-01-05");
  CHECK(format_date(ymd(5, 12, 31)) == "0005-12-31");
  CHECK(format_month_key(ym(2026, 11)) == "2026-11");
  CHECK(format_month_label(ym(2026, 11)) == "Nov 2026");
  CHECK(format_month_label(ym(2026, 1)) == "Jan 2026");
  CHECK(parse_date(format_date(ymd(2026, 2, 28))) == ymd(2026, 2, 28));
  CHECK(parse_year_month(format_month_key(ym(1999, 7))) == ym(1999, 7));
}

TEST_CASE("month_of and day keys", "[calendar]") {
  CHECK(month_of(ymd(2026, 11, 26)) == ym(2026, 11));
  CHECK(first_day_key(ym(2026, 11)) == "2026-11-01");
  CHECK(last_day_key(ym(2026, 11)) == "2026-11-31"); // lexicographic bound
  // Every real day of a month lies between its bounds, nothing else does.
  CHECK(format_date(ymd(2026, 2, 28)) <= last_day_key(ym(2026, 2)));
  CHECK(format_date(ymd(2026, 3, 1)) > last_day_key(ym(2026, 2)));
  CHECK(format_date(ymd(2025, 12, 31)) < first_day_key(ym(2026, 1)));
}

TEST_CASE("in_supported_range covers years 0000-9999", "[calendar]") {
  CHECK(in_supported_range(ym(0, 1)));
  CHECK(in_supported_range(ym(9999, 12)));
  CHECK_FALSE(in_supported_range(ym(0, 1) - std::chrono::months{1}));
  CHECK_FALSE(in_supported_range(ym(9999, 12) + std::chrono::months{1}));
}

TEST_CASE("today is a valid date", "[calendar]") {
  const Date d = today();
  CHECK(d.ok());
  CHECK(d.year() >= std::chrono::year{2025});
}
