#include "db/entry_repository.hpp"
#include "support.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <stdexcept>

using namespace fin;
using fin::test::TempDb;
using fin::test::ym;
using Catch::Matchers::ContainsSubstring;

namespace {

/// The schema as the very first version of the program created it
/// (user_version 0, no date or category-list constraints on `date`).
constexpr auto kLegacySchema = R"sql(
  CREATE TABLE entries (id INTEGER PRIMARY KEY AUTOINCREMENT, title TEXT NOT NULL,
    amount_cents INTEGER NOT NULL CHECK (amount_cents > 0),
    kind TEXT NOT NULL CHECK (kind IN ('expense','income')),
    category TEXT NOT NULL CHECK (category IN ('alimentacion','transporte','vivienda','ocio','otros')),
    date TEXT NOT NULL, created_at TEXT NOT NULL DEFAULT (datetime('now')));
  CREATE INDEX entries_by_date ON entries(date);
  CREATE TABLE people (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT NOT NULL UNIQUE);
  CREATE TABLE entry_shares (
    entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
    person_id INTEGER NOT NULL REFERENCES people(id) ON DELETE CASCADE,
    amount_cents INTEGER NOT NULL CHECK (amount_cents > 0),
    PRIMARY KEY (entry_id, person_id));
  INSERT INTO entries (title,amount_cents,kind,category,date) VALUES
    ('a',1000,'expense','otros','2026-11-01'),
    ('b',500,'income','otros','2026-11-02'),
    ('c',700,'expense','ocio','2026-11-03');
  DELETE FROM entries WHERE id = 3;  -- AUTOINCREMENT must remember id 3
  INSERT INTO people (name) VALUES ('Ana');
  INSERT INTO entry_shares VALUES (1, 1, 300);
)sql";

int user_version(SQLite::Database &db) {
  return db.execAndGet("PRAGMA user_version").getInt();
}

bool fails(SQLite::Database &db, const char *sql) {
  try {
    db.exec(sql);
    return false;
  } catch (const SQLite::Exception &) {
    return true;
  }
}

} // namespace

TEST_CASE("a new database gets the current schema", "[migrations]") {
  const TempDb file;
  { const EntryRepository repo{file.path()}; }
  auto db = file.raw();
  CHECK(user_version(db) == 3);
  CHECK(fails(db, "INSERT INTO entries (title,amount_cents,kind,category,date) "
                  "VALUES ('x',1,'expense','otros','garbage')"));
}

TEST_CASE("a legacy database is migrated keeping its data", "[migrations]") {
  const TempDb file;
  { auto db = file.raw(); db.exec(kLegacySchema); }

  {
    EntryRepository repo{file.path()};
    CHECK(repo.entries_in(ym(2026, 11)).size() == 2);
    REQUIRE(repo.shares(1).size() == 1); // survived the table rebuild
    CHECK(repo.shares(1)[0].amount == 300);
    repo.add(fin::test::make_entry("n", 100, Kind::expense, Category::other,
                                   fin::test::ymd(2026, 11, 4)));
  }

  auto db = file.raw();
  CHECK(user_version(db) == 3);
  CHECK(db.execAndGet("SELECT MAX(id) FROM entries").getInt() == 4); // id 3 not reused
  CHECK(fails(db, "INSERT INTO entries (title,amount_cents,kind,category,date) "
                  "VALUES ('x',1,'expense','otros','2026-1-5')"));
  // The category list lives in code only: new categories need no migration.
  CHECK_FALSE(fails(db, "INSERT INTO entries (title,amount_cents,kind,category,date) "
                        "VALUES ('x',1,'expense','future','2026-11-05')"));

  { const EntryRepository again{file.path()}; } // opening again is a no-op
  CHECK(user_version(db) == 3);
}

TEST_CASE("a legacy database with a malformed date is left untouched", "[migrations]") {
  const TempDb file;
  {
    auto db = file.raw();
    db.exec(kLegacySchema);
    db.exec("UPDATE entries SET date = 'garbage' WHERE id = 1");
  }
  CHECK_THROWS_WITH(EntryRepository{file.path()}, ContainsSubstring("CHECK constraint failed"));

  auto db = file.raw();
  CHECK(user_version(db) == 1); // migration 2 rolled back as a whole
  CHECK(db.execAndGet("SELECT COUNT(*) FROM entries").getInt() == 2);
  CHECK(db.execAndGet("SELECT date FROM entries WHERE id = 1").getString() == "garbage");
}

TEST_CASE("a database from a newer version is refused", "[migrations]") {
  const TempDb file;
  { const EntryRepository repo{file.path()}; }
  { auto db = file.raw(); db.exec("PRAGMA user_version = 99"); }
  CHECK_THROWS_WITH(EntryRepository{file.path()}, ContainsSubstring("newer version"));
}

TEST_CASE("corrupt stored values are reported, not reinterpreted", "[migrations]") {
  const TempDb file;
  EntryRepository repo{file.path()};
  file.raw().exec("INSERT INTO entries (title,amount_cents,kind,category,date) "
                  "VALUES ('x',1,'expense','future','2026-11-05')");
  CHECK_THROWS_WITH(repo.entries_in(ym(2026, 11)),
                    ContainsSubstring("Invalid category"));
  CHECK_THROWS_WITH(repo.expenses_by_category(ym(2026, 11), ym(2026, 11)),
                    ContainsSubstring("Invalid category"));
}
