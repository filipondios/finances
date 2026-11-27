#include "core/entry_form.hpp"
#include "core/errors.hpp"
#include "support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <string>

using namespace fin;
using fin::test::ymd;

namespace {
EntryForm form_with_title(std::string_view title) {
  return {.title = title, .amount = "12,50", .kind = "expense",
          .category = "alimentacion", .date = "2026-11-15",
          .month = "2026-11"};
}
std::string repeat(std::string_view unit, int times) {
  std::string out;
  for (int i = 0; i < times; ++i)
    out += unit;
  return out;
}
} // namespace

TEST_CASE("validate builds an entry from a good form", "[form]") {
  const NewEntry e = validate(form_with_title("  Compra  "));
  CHECK(e.title == "Compra");
  CHECK(e.amount == 1250);
  CHECK(e.kind == Kind::expense);
  CHECK(e.category == Category::food);
  CHECK(e.date == ymd(2026, 11, 15));
}

TEST_CASE("validate limits titles by characters, not bytes", "[form]") {
  CHECK_NOTHROW(validate(form_with_title(repeat("ñ", 100)))); // 200 bytes
  CHECK_THROWS_WITH(validate(form_with_title(repeat("ñ", 101))),
                    "The title is required (maximum 100 characters).");
  CHECK_THROWS_WITH(validate(form_with_title("   ")),
                    "The title is required (maximum 100 characters).");
}

TEST_CASE("validate rejects invalid characters in titles", "[form]") {
  CHECK_THROWS_WITH(validate(form_with_title("a\tb")),
                    "The title contains invalid characters.");
  CHECK_THROWS_WITH(validate(form_with_title("\xC0\xAF")),
                    "The title contains invalid characters.");
}

TEST_CASE("validate reports each bad field", "[form]") {
  auto f = form_with_title("x");
  f.kind = "gift";
  CHECK_THROWS_WITH(validate(f), "Select expense or income.");

  f = form_with_title("x");
  f.category = "unknown";
  CHECK_THROWS_WITH(validate(f), "Select a category.");

  f = form_with_title("x");
  f.date = "2026-02-30";
  CHECK_THROWS_WITH(validate(f), "The date must belong to the displayed month.");

  f = form_with_title("x");
  f.date = "2026-10-31"; // real date, other month
  CHECK_THROWS_WITH(validate(f), "The date must belong to the displayed month.");

  f = form_with_title("x");
  f.amount = "abc";
  CHECK_THROWS_AS(validate(f), ValidationError);
}
