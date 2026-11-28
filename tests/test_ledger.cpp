#include "app/ledger.hpp"
#include "core/errors.hpp"
#include "db/entry_repository.hpp"
#include "support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

using namespace fin;
using fin::test::make_entry;
using fin::test::ym;
using fin::test::ymd;

namespace {

constexpr auto kExpense = Kind::expense;
constexpr auto kIncome = Kind::income;
const Date kNov1 = ymd(2026, 11, 1);

/// In-memory ledger with a fixed "today" (2026-11-15).
struct Fixture {
  EntryRepository repo{":memory:"};
  Ledger ledger{repo, [] { return ymd(2026, 11, 15); }};
};

} // namespace

TEST_CASE("entries are listed newest date first, ties by newest id", "[repo]") {
  EntryRepository repo{":memory:"};
  repo.add(make_entry("a", 100, kExpense, Category::food, ymd(2026, 11, 1)));
  repo.add(make_entry("b", 200, kExpense, Category::food, ymd(2026, 11, 3)));
  repo.add(make_entry("c", 300, kIncome, Category::other, ymd(2026, 11, 3)));

  const auto entries = repo.entries_in(ym(2026, 11));
  REQUIRE(entries.size() == 3);
  CHECK(entries[0].title == "c");
  CHECK(entries[1].title == "b");
  CHECK(entries[2].title == "a");
  CHECK(entries[0].kind == Kind::income);
  CHECK(entries[1].date == ymd(2026, 11, 3));
}

TEST_CASE("a month only contains its own days", "[repo]") {
  EntryRepository repo{":memory:"};
  repo.add(make_entry("oct31", 1, kExpense, Category::food, ymd(2026, 10, 31)));
  repo.add(make_entry("nov1", 1, kExpense, Category::food, ymd(2026, 11, 1)));
  repo.add(make_entry("nov30", 1, kExpense, Category::food, ymd(2026, 11, 30)));
  repo.add(make_entry("dec1", 1, kExpense, Category::food, ymd(2026, 12, 1)));

  const auto entries = repo.entries_in(ym(2026, 11));
  REQUIRE(entries.size() == 2);
  CHECK(entries[0].title == "nov30");
  CHECK(entries[1].title == "nov1");
}

TEST_CASE("remove returns the month, then nothing", "[repo]") {
  EntryRepository repo{":memory:"};
  repo.add(make_entry("a", 100, kExpense, Category::food, ymd(2026, 11, 9)));
  const auto id = repo.entries_in(ym(2026, 11)).front().id;

  CHECK(repo.remove(id) == ym(2026, 11));
  CHECK_FALSE(repo.remove(id).has_value());
  CHECK(repo.entries_in(ym(2026, 11)).empty());
}

TEST_CASE("totals, categories and months are aggregated by the database",
          "[repo]") {
  EntryRepository repo{":memory:"};
  repo.add(make_entry("s", 1000, kIncome, Category::other, ymd(2026, 11, 1)));
  repo.add(make_entry("f", 300, kExpense, Category::food, ymd(2026, 11, 2)));
  repo.add(
      make_entry("t", 200, kExpense, Category::transport, ymd(2026, 11, 3)));
  repo.add(make_entry("o", 50, kExpense, Category::food, ymd(2026, 9, 3)));

  const auto totals = repo.totals_between(ym(2026, 9), ym(2026, 11));
  REQUIRE(totals.size() == 2); // October has no entries: absent
  CHECK(totals.at(ym(2026, 11)).income == 1000);
  CHECK(totals.at(ym(2026, 11)).expenses == 500);
  CHECK(totals.at(ym(2026, 11)).count == 3);
  CHECK(totals.at(ym(2026, 9)).expenses == 50);

  const auto by_category =
      repo.expenses_by_category(ym(2026, 11), ym(2026, 11));
  CHECK(by_category[static_cast<std::size_t>(Category::food)] == 300);
  CHECK(by_category[static_cast<std::size_t>(Category::transport)] == 200);
  CHECK(by_category[static_cast<std::size_t>(Category::other)] == 0); // income

  const auto months = repo.months_with_entries();
  REQUIRE(months.size() == 2);
  CHECK(months[0] == ym(2026, 11)); // newest first
  CHECK(months[1] == ym(2026, 9));
}

TEST_CASE("Ledger::add_entry validates and returns the month", "[ledger]") {
  Fixture f;
  const auto month = f.ledger.add_entry({.title = "Compra",
                                         .amount = "1.234,56",
                                         .kind = "expense",
                                         .category = "ocio",
                                         .date = "2026-11-20",
                                         .month = "2026-11"});
  CHECK(month == ym(2026, 11));
  const auto entries = f.repo.entries_in(ym(2026, 11));
  REQUIRE(entries.size() == 1);
  CHECK(entries[0].amount == 123456);
  CHECK(entries[0].category == Category::leisure);

  CHECK_THROWS_AS(f.ledger.add_entry({.title = "",
                                      .amount = "1",
                                      .kind = "expense",
                                      .category = "ocio",
                                      .date = "2026-11-20",
                                      .month = "2026-11"}),
                  ValidationError);
  CHECK(f.repo.entries_in(ym(2026, 11)).size() == 1); // nothing was stored
}

TEST_CASE("people: names are validated and unique", "[ledger]") {
  Fixture f;
  CHECK(f.ledger.add_person("  Ana  ") == 1);
  CHECK_THROWS_WITH(f.ledger.add_person("Ana"), "That person already exists.");
  CHECK_THROWS_WITH(f.ledger.add_person("  "),
                    "The name is required (maximum 50 characters).");
  CHECK_THROWS_WITH(f.ledger.add_person(std::string(51, 'x')),
                    "The name is required (maximum 50 characters).");
  CHECK_THROWS_WITH(f.ledger.add_person("a\tb"),
                    "The name contains invalid characters.");
  (void)f.ledger.add_person("Aaron");

  const auto people = f.ledger.people();
  REQUIRE(people.size() == 2);
  CHECK(people[0].name == "Aaron"); // ordered by name
  CHECK(people[1].name == "Ana");
}

TEST_CASE("shares never exceed the entry total", "[ledger][shares]") {
  Fixture f;
  f.repo.add(make_entry("dinner", 1000, kExpense, Category::food, kNov1));
  const auto entry = f.repo.entries_in(ym(2026, 11)).front().id;
  const auto ana = f.ledger.add_person("Ana");
  const auto bob = f.ledger.add_person("Bob");

  f.ledger.add_share(entry, ana, 600);
  CHECK_THROWS_WITH(f.ledger.add_share(entry, bob, 401),
                    "The shared amount exceeds the entry total.");
  CHECK_NOTHROW(f.ledger.add_share(entry, bob, 400)); // exactly the rest
  CHECK(f.ledger.shares(entry).size() == 2);
}

TEST_CASE("share errors are specific", "[ledger][shares]") {
  Fixture f;
  f.repo.add(make_entry("dinner", 1000, kExpense, Category::food, kNov1));
  const auto entry = f.repo.entries_in(ym(2026, 11)).front().id;
  const auto ana = f.ledger.add_person("Ana");
  f.ledger.add_share(entry, ana, 100);

  CHECK_THROWS_WITH(
      f.ledger.add_share(entry, ana, 100),
      "That person already has a share in this entry. Remove it first.");
  CHECK_THROWS_WITH(f.ledger.add_share(999, ana, 100), "Entry not found.");
  CHECK_THROWS_WITH(f.ledger.add_share(entry, 777, 100), "Select a person.");
  CHECK_THROWS_WITH(f.ledger.add_share(entry, ana, 0),
                    "The amount is not valid.");
  CHECK(f.ledger.shares(entry).size() == 1);
}

TEST_CASE("a failed share does not leave a new person behind",
          "[ledger][shares]") {
  Fixture f;
  f.repo.add(make_entry("dinner", 1000, kExpense, Category::food, kNov1));
  const auto entry = f.repo.entries_in(ym(2026, 11)).front().id;

  CHECK_THROWS_WITH(f.ledger.add_share_with_new_person(entry, "Ghost", 1001),
                    "The shared amount exceeds the entry total.");
  CHECK_THROWS_WITH(f.ledger.add_share_with_new_person(999, "Ghost", 100),
                    "Entry not found.");
  CHECK(f.ledger.people().empty()); // rolled back

  f.ledger.add_share_with_new_person(entry, "  Bob ", 250);
  REQUIRE(f.ledger.people().size() == 1);
  const auto shares = f.ledger.shares(entry);
  REQUIRE(shares.size() == 1);
  CHECK(shares[0].name == "Bob");
  CHECK(shares[0].amount == 250);

  CHECK_THROWS_WITH(f.ledger.add_share_with_new_person(entry, "Bob", 10),
                    "That person already exists.");
  CHECK(f.ledger.shares(entry).size() == 1);
}

TEST_CASE("deleting a person or an entry removes its shares",
          "[ledger][shares]") {
  Fixture f;
  f.repo.add(make_entry("a", 1000, kExpense, Category::food, kNov1));
  f.repo.add(make_entry("b", 1000, kExpense, Category::food, kNov1));
  const auto entries = f.repo.entries_in(ym(2026, 11));
  const auto ana = f.ledger.add_person("Ana");
  f.ledger.add_share(entries[0].id, ana, 100);
  f.ledger.add_share(entries[1].id, ana, 100);

  CHECK(f.ledger.remove_entry(entries[0].id).has_value());
  CHECK(f.ledger.shares(entries[0].id).empty());
  CHECK(f.ledger.shares(entries[1].id).size() == 1);

  f.ledger.remove_person(ana);
  CHECK(f.ledger.shares(entries[1].id).empty());
}

TEST_CASE("debts are summed overall and per month", "[ledger][shares]") {
  Fixture f;
  f.repo.add(
      make_entry("nov", 1000, kExpense, Category::food, ymd(2026, 11, 2)));
  f.repo.add(
      make_entry("oct", 500, kExpense, Category::food, ymd(2026, 10, 2)));
  const auto nov = f.repo.entries_in(ym(2026, 11)).front().id;
  const auto oct = f.repo.entries_in(ym(2026, 10)).front().id;
  const auto ana = f.ledger.add_person("Ana");
  const auto bob = f.ledger.add_person("Bob");
  (void)f.ledger.add_person("Carl"); // owes nothing
  f.ledger.add_share(nov, ana, 300);
  f.ledger.add_share(oct, ana, 200);
  f.ledger.add_share(nov, bob, 400);

  const auto debts = f.ledger.debts(ym(2026, 11));
  REQUIRE(debts.size() == 3);
  CHECK(debts[0].name == "Ana"); // largest total first
  CHECK(debts[0].total == 500);
  CHECK(debts[0].month_total == 300);
  CHECK(debts[1].name == "Bob");
  CHECK(debts[1].total == 400);
  CHECK(debts[1].month_total == 400);
  CHECK(debts[2].name == "Carl");
  CHECK(debts[2].total == 0);

  const auto counts = f.ledger.share_counts(ym(2026, 11));
  CHECK(counts.at(nov) == 2);
  CHECK_FALSE(counts.contains(oct)); // other month
}

TEST_CASE("payments reduce debts and never make them negative",
          "[ledger][settlements]") {
  Fixture f;
  f.repo.add(
      make_entry("shared", 1000, kExpense, Category::food, ymd(2026, 11, 2)));
  const auto entry = f.repo.entries_in(ym(2026, 11)).front().id;
  const auto ana = f.ledger.add_person("Ana");
  f.ledger.add_share(entry, ana, 500);

  f.ledger.add_settlement(entry, ana, 200);
  auto debts = f.ledger.debts(ym(2026, 11));
  REQUIRE(debts.size() == 1);
  CHECK(debts[0].total == 300);
  CHECK(debts[0].month_total == 300);

  // A netting payment can exceed the recorded debt: the visible debt is zero.
  f.ledger.add_settlement(entry, ana, 900);
  debts = f.ledger.debts(ym(2026, 11));
  CHECK(debts[0].total == 0);
  CHECK(debts[0].month_total == 0);

  std::int64_t big_id = 0;
  for (const auto &payment : f.ledger.settlements(entry))
    if (payment.amount == 900)
      big_id = payment.id;
  REQUIRE(big_id != 0);
  f.ledger.remove_settlement(big_id);
  CHECK(f.ledger.debts(ym(2026, 11))[0].total == 300);
}

TEST_CASE("the clock is injectable", "[ledger]") {
  Fixture f;
  CHECK(f.ledger.today() == ymd(2026, 11, 15));
  EntryRepository repo{":memory:"};
  CHECK(Ledger{repo}.today().ok()); // default: the real date
}

TEST_CASE("dashboard covers the last six months ending at 'current'",
          "[ledger]") {
  EntryRepository repo{":memory:"};
  Ledger ledger{repo, [] { return ymd(2030, 2, 28); }};
  repo.add(make_entry("pay", 1000, kIncome, Category::other, ymd(2030, 2, 1)));
  repo.add(make_entry("food", 300, kExpense, Category::food, ymd(2030, 2, 5)));
  repo.add(
      make_entry("bus", 500, kExpense, Category::transport, ymd(2029, 9, 5)));
  repo.add(make_entry("old", 999, kExpense, Category::other, ymd(2029, 8, 5)));

  const auto d = ledger.dashboard(month_of(ledger.today()));
  REQUIRE(d.history.size() == 6);
  CHECK(d.history.front().month == ym(2029, 9));
  CHECK(d.history.front().totals.expenses == 500);
  CHECK(d.history[1].totals.count == 0); // empty months are present
  CHECK(d.history.back().month == ym(2030, 2));
  CHECK(d.current.totals.income == 1000);
  CHECK(d.current.totals.expenses == 300);
  REQUIRE(d.current.by_category.size() == 1);
  CHECK(d.current.by_category[0].percent == 100);

  REQUIRE(d.history_by_category.size() == 2); // August is outside the window
  CHECK(d.history_by_category[0].category == Category::transport);
  CHECK(d.history_by_category[0].percent == 63); // 500 / 800 = 62.5, half up
  CHECK(d.history_by_category[1].percent == 38);
}

TEST_CASE("selectable months: data months plus the current one, no duplicates",
          "[ledger]") {
  Fixture f;
  f.repo.add(make_entry("a", 1, kExpense, Category::food, ymd(2026, 9, 1)));
  f.repo.add(make_entry("b", 1, kExpense, Category::food, ymd(2026, 9, 2)));
  f.repo.add(make_entry("c", 1, kExpense, Category::food, ymd(2026, 11, 1)));

  CHECK(f.ledger.selectable_months(ym(2026, 12)) ==
        std::vector{ym(2026, 12), ym(2026, 11), ym(2026, 9)});
  CHECK(f.ledger.selectable_months(ym(2026, 11)) ==
        std::vector{ym(2026, 11), ym(2026, 9)});
  CHECK(Fixture{}.ledger.selectable_months(ym(2026, 11)) ==
        std::vector{ym(2026, 11)});
}
