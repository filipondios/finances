#include "core/report.hpp"
#include "support.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace fin;
using fin::test::ym;
using fin::test::ymd;

namespace {
CategoryTotals totals(Cents food, Cents transport, Cents housing, Cents leisure,
                      Cents other) {
  return {food, transport, housing, leisure, other};
}
} // namespace

TEST_CASE("rank_categories sorts by spend and computes percentages",
          "[report]") {
  const auto ranked = rank_categories(totals(600, 300, 0, 100, 0));
  REQUIRE(ranked.size() == 3);
  CHECK(ranked[0].category == Category::food);
  CHECK(ranked[0].percent == 60);
  CHECK(ranked[1].category == Category::transport);
  CHECK(ranked[1].percent == 30);
  CHECK(ranked[2].category == Category::leisure);
  CHECK(ranked[2].amount == 100);
  CHECK(ranked[2].percent == 10);
}

TEST_CASE("rank_categories breaks ties by enum order", "[report]") {
  const auto ranked = rank_categories(totals(0, 100, 100, 0, 100));
  REQUIRE(ranked.size() == 3);
  CHECK(ranked[0].category == Category::transport);
  CHECK(ranked[1].category == Category::housing);
  CHECK(ranked[2].category == Category::other);
}

TEST_CASE("rank_categories rounds half up and may not add to 100", "[report]") {
  const auto halves = rank_categories(totals(1, 7, 0, 0, 0)); // 12.5% / 87.5%
  REQUIRE(halves.size() == 2);
  CHECK(halves[0].percent == 88);
  CHECK(halves[1].percent == 13);

  const auto thirds = rank_categories(totals(1, 1, 1, 0, 0));
  REQUIRE(thirds.size() == 3);
  for (const auto &share : thirds)
    CHECK(share.percent == 33);
}

TEST_CASE("rank_categories with no expenses is empty", "[report]") {
  CHECK(rank_categories(totals(0, 0, 0, 0, 0)).empty());
}

TEST_CASE("make_month_report totals a month in one pass", "[report]") {
  std::vector<Entry> entries{
      {.id = 3,
       .title = "salary",
       .amount = 1000,
       .kind = Kind::income,
       .category = Category::other,
       .date = ymd(2026, 11, 1)},
      {.id = 2,
       .title = "lunch",
       .amount = 300,
       .kind = Kind::expense,
       .category = Category::food,
       .date = ymd(2026, 11, 2)},
      {.id = 1,
       .title = "misc",
       .amount = 200,
       .kind = Kind::expense,
       .category = Category::other,
       .date = ymd(2026, 11, 3)},
  };
  const auto report = make_month_report(ym(2026, 11), entries);
  CHECK(report.summary.month == ym(2026, 11));
  CHECK(report.summary.totals.income == 1000);
  CHECK(report.summary.totals.expenses == 500);
  CHECK(report.summary.totals.balance() == 500);
  CHECK(report.summary.totals.count == 3);
  REQUIRE(report.summary.by_category.size() == 2); // income is not a category
  CHECK(report.summary.by_category[0].category == Category::food);
  CHECK(report.summary.by_category[0].percent == 60);
  CHECK(report.summary.by_category[1].percent == 40);
  CHECK(report.entries.size() == 3);
  CHECK(report.entries.front().id == 3); // order is preserved
}

TEST_CASE("categories table is consistent", "[types]") {
  CHECK(category_from_key("ocio") == Category::leisure);
  CHECK_FALSE(category_from_key("nope").has_value());
  CHECK(kind_from_key("income") == Kind::income);
  CHECK_FALSE(kind_from_key("").has_value());
}
