#include "core/errors.hpp"
#include "core/money.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <cstdint>
#include <limits>
#include <string_view>

using namespace fin;

TEST_CASE("parse_amount accepts", "[money]") {
  const auto [text, cents] = GENERATE(table<std::string_view, Cents>({
      {"12", 1200},
      {" 12 ", 1200},
      {"12,5", 1250},
      {"12.50", 1250},
      {"0,05", 5},
      {".5", 50},
      {"5,", 500},
      {"1.234,56", 123456},
      {"1,234.56", 123456},
      {"1.234.567,89", 123456789},
      {"1,234,567.89", 123456789},
      {"1.234.567", 123456700},
      {"1,234,567", 123456700},
      {"999.999.999.999", 99999999999900},
  }));
  CHECK(parse_amount(text) == cents);
}

TEST_CASE("parse_amount rejects", "[money]") {
  const auto text = GENERATE(as<std::string_view>{}, "abc", "-5", "+5", "0",
                             "0,00", ",", ".,", "1.234", // ambiguous
                             "1,234", "12.345.67", "1.2,3", "1,2.3", "1..5",
                             "1.234,567,89", "1.23.4", "1234.567,89",
                             "1.234,567", "12,345", "1000.000.000.000",
                             "12345678901234"); // more than 12 whole digits
  CHECK_THROWS_WITH(parse_amount(text), "The amount is not valid.");
}

TEST_CASE("parse_amount asks for an amount when blank", "[money]") {
  CHECK_THROWS_WITH(parse_amount(""), "Enter an amount.");
  CHECK_THROWS_WITH(parse_amount("  \t"), "Enter an amount.");
  CHECK_THROWS_AS(parse_amount(""), ValidationError);
}

TEST_CASE("format_money uses the Spanish format", "[money]") {
  CHECK(format_money(0) == "0,00 €");
  CHECK(format_money(5) == "0,05 €");
  CHECK(format_money(100) == "1,00 €");
  CHECK(format_money(123456) == "1.234,56 €");
  CHECK(format_money(123456789) == "1.234.567,89 €");
  CHECK(format_money(-500) == "-5,00 €");
  CHECK(format_money(-123456) == "-1.234,56 €");
}

TEST_CASE("format_money does not overflow at the extremes", "[money]") {
  CHECK(format_money(std::numeric_limits<std::int64_t>::min()) ==
        "-92.233.720.368.547.758,08 €");
  CHECK(format_money(std::numeric_limits<std::int64_t>::max()) ==
        "92.233.720.368.547.758,07 €");
}

TEST_CASE("format_money output can be parsed back", "[money]") {
  for (const Cents c : {1, 99, 100, 12345, 123456, 99999999}) {
    const auto text = format_money(c);
    CHECK(parse_amount(text.substr(0, text.size() -
                                          std::string_view{" €"}.size())) == c);
  }
}
