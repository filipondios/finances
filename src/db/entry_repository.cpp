#include "db/entry_repository.hpp"
#include "core/calendar.hpp"
#include "core/errors.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <sqlite3.h>
#include <array>
#include <stdexcept>
#include <string>
#include <string_view>

namespace fin {
namespace {

/// Migration 1: the original schema. Frozen: never edit it, append a migration.
constexpr auto kMigration1 = R"sql(
    CREATE TABLE IF NOT EXISTS entries (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        title TEXT NOT NULL,
        amount_cents INTEGER NOT NULL CHECK (amount_cents > 0),
        kind TEXT NOT NULL CHECK (kind IN ('expense', 'income')),
        category TEXT NOT NULL CHECK (category IN ('alimentacion', 'transporte', 'vivienda', 'ocio', 'otros')),
        date TEXT NOT NULL,
        created_at TEXT NOT NULL DEFAULT (datetime('now'))
    );
    CREATE INDEX IF NOT EXISTS entries_by_date ON entries(date);
    CREATE TABLE IF NOT EXISTS people (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        name TEXT NOT NULL UNIQUE
    );
    CREATE TABLE IF NOT EXISTS entry_shares (
        entry_id INTEGER NOT NULL REFERENCES entries(id) ON DELETE CASCADE,
        person_id INTEGER NOT NULL REFERENCES people(id) ON DELETE CASCADE,
        amount_cents INTEGER NOT NULL CHECK (amount_cents > 0),
        PRIMARY KEY (entry_id, person_id)
    );
)sql";

/// Migration 2: `entries.date` must look like YYYY-MM-DD. SQLite cannot add a
/// CHECK to an existing table, so the table is rebuilt (the procedure from the
/// SQLite docs; it runs with foreign keys off so the shares are not cascaded).
/// It fails, rolling back, if a legacy row has a malformed date.
constexpr auto kMigration2 = R"sql(
    CREATE TABLE entries_new (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        title TEXT NOT NULL,
        amount_cents INTEGER NOT NULL CHECK (amount_cents > 0),
        kind TEXT NOT NULL CHECK (kind IN ('expense', 'income')),
        category TEXT NOT NULL CHECK (category IN ('alimentacion', 'transporte', 'vivienda', 'ocio', 'otros')),
        date TEXT NOT NULL CHECK (date GLOB '[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]'),
        created_at TEXT NOT NULL DEFAULT (datetime('now'))
    );
    INSERT INTO entries_new (id, title, amount_cents, kind, category, date, created_at)
        SELECT id, title, amount_cents, kind, category, date, created_at FROM entries;
    UPDATE sqlite_sequence SET seq = MAX(seq, COALESCE(
        (SELECT seq FROM sqlite_sequence WHERE name = 'entries'), 0))
        WHERE name = 'entries_new';
    DROP TABLE entries;
    ALTER TABLE entries_new RENAME TO entries;
    CREATE INDEX entries_by_date ON entries(date);
)sql";

constexpr std::array<std::string_view, 2> kMigrations{kMigration1, kMigration2};

/// Applies the pending migrations, each in its own transaction, tracking the
/// schema version in `PRAGMA user_version`.
void migrate(SQLite::Database &db) {
  const auto applied =
      static_cast<std::size_t>(db.execAndGet("PRAGMA user_version").getInt());
  if (applied > kMigrations.size())
    throw std::runtime_error(
        "finances.db was created by a newer version of this program.");
  for (auto i = applied; i < kMigrations.size(); ++i) {
    SQLite::Transaction tx{db};
    db.exec(std::string{kMigrations[i]});
    db.exec("PRAGMA user_version = " + std::to_string(i + 1));
    tx.commit();
  }
}

SQLite::Database open_database(const std::filesystem::path &file) {
  SQLite::Database db{file, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE};
  db.setBusyTimeout(5'000);
  // WAL + FULL: no committed transaction is lost on power failure. The write
  // volume of a personal ledger makes the extra fsync irrelevant.
  db.exec("PRAGMA journal_mode = WAL; PRAGMA synchronous = FULL;");
  // Off while migrating (table rebuilds must not cascade), then on for good.
  db.exec("PRAGMA foreign_keys = OFF;");
  migrate(db);
  db.exec("PRAGMA foreign_keys = ON;");
  return db;
}

class ResetOnExit {
public:
  explicit ResetOnExit(SQLite::Statement &s) noexcept : stmt_{s} {}
  ~ResetOnExit() { stmt_.tryReset(); }
  ResetOnExit(const ResetOnExit &) = delete;
  ResetOnExit &operator=(const ResetOnExit &) = delete;

private:
  SQLite::Statement &stmt_;
};

/// Text of a column without copying it (valid until the next step/reset).
std::string_view text_of(const SQLite::Column &c) noexcept {
  return c.getText();
}

/// Stored values are guaranteed by CHECK constraints; anything else is
/// corruption and must not be silently reinterpreted.
Kind stored_kind(std::string_view s) {
  if (const auto k = kind_from_key(s))
    return *k;
  throw std::runtime_error("Invalid kind in the database: " + std::string{s});
}

Category stored_category(std::string_view s) {
  if (const auto c = category_from_key(s))
    return *c;
  throw std::runtime_error("Invalid category in the database: " +
                           std::string{s});
}

bool is_constraint_violation(const SQLite::Exception &e) noexcept {
  return e.getErrorCode() == SQLITE_CONSTRAINT;
}

Date parse_stored_date(std::string_view s) {
  if (const auto d = parse_date(s))
    return *d;
  throw std::runtime_error("Invalid date in the database: " +
                           std::string{s});
}

} // namespace

EntryRepository::EntryRepository(const std::filesystem::path &file)
    : db_{open_database(file)},
      insert_{db_, "INSERT INTO entries (title, amount_cents, kind, category, "
                   "date) VALUES (?1, ?2, ?3, ?4, ?5)"},
      remove_{db_, "DELETE FROM entries WHERE id = ? RETURNING substr(date, 1, 7)"},
      // `date` is "YYYY-MM-DD": comparing against the month range lets the
      // query use the entries_by_date index (with substr(date,1,7)=? it would
      // have to scan the whole table).
      list_{db_, "SELECT id, title, amount_cents, kind, category, date "
                 "FROM entries WHERE date >= ?1 AND date <= ?2 "
                 "ORDER BY date DESC, id DESC"},
      totals_{db_, "SELECT substr(date, 1, 7), kind, SUM(amount_cents), "
                   "COUNT(*) FROM entries WHERE date >= ?1 AND date <= ?2 "
                   "GROUP BY 1, 2"},
      by_category_{db_, "SELECT category, SUM(amount_cents) FROM entries "
                        "WHERE date >= ?1 AND date <= ?2 AND kind = 'expense' "
                        "GROUP BY category"},
      months_{db_, "SELECT DISTINCT substr(date, 1, 7) FROM entries "
                   "ORDER BY 1 DESC"},
      people_{db_, "SELECT id, name FROM people ORDER BY name"},
      add_person_{db_, "INSERT INTO people (name) VALUES (?)"},
      remove_person_{db_, "DELETE FROM people WHERE id = ?"},
      share_counts_{db_, "SELECT s.entry_id, COUNT(*) FROM entry_shares s "
                          "JOIN entries e ON e.id = s.entry_id "
                          "WHERE e.date >= ?1 AND e.date <= ?2 GROUP BY s.entry_id"},
      shares_{db_, "SELECT s.person_id, p.name, s.amount_cents "
                   "FROM entry_shares s JOIN people p ON p.id = s.person_id "
                   "WHERE s.entry_id = ? ORDER BY p.name"},
      share_remaining_{db_, "SELECT e.amount_cents - COALESCE("
                             "(SELECT SUM(amount_cents) FROM entry_shares "
                             "WHERE entry_id = e.id), 0) FROM entries e WHERE e.id = ?"},
      person_exists_{db_, "SELECT 1 FROM people WHERE id = ?"},
      add_share_{db_, "INSERT INTO entry_shares (entry_id, person_id, amount_cents) "
                      "VALUES (?1, ?2, ?3)"},
      remove_share_{db_, "DELETE FROM entry_shares "
                         "WHERE entry_id = ?1 AND person_id = ?2"},
      debts_{db_, "SELECT p.id, p.name, COALESCE(SUM(s.amount_cents), 0), "
                  "COALESCE(SUM(CASE WHEN e.date >= ?1 AND e.date <= ?2 "
                  "THEN s.amount_cents ELSE 0 END), 0) "
                  "FROM people p LEFT JOIN entry_shares s ON s.person_id = p.id "
                  "LEFT JOIN entries e ON e.id = s.entry_id "
                  "GROUP BY p.id ORDER BY 3 DESC, p.name"} {}

void EntryRepository::add(const NewEntry &entry) {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{insert_};
  insert_.bind(1, entry.title);
  insert_.bind(2, entry.amount);
  insert_.bind(3, std::string{key(entry.kind)});
  insert_.bind(4, std::string{info(entry.category).key});
  insert_.bind(5, format_date(entry.date));
  insert_.exec();
}

std::optional<YearMonth> EntryRepository::remove(std::int64_t id) {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{remove_};
  remove_.bind(1, id);
  if (!remove_.executeStep())
    return std::nullopt;
  return parse_year_month(text_of(remove_.getColumn(0)));
}

std::vector<Entry> EntryRepository::entries_in(YearMonth month) const {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{list_};
  list_.bind(1, first_day_key(month));
  list_.bind(2, last_day_key(month));

  std::vector<Entry> out;
  while (list_.executeStep()) {
    Entry e;
    e.id = list_.getColumn(0).getInt64();
    e.title = list_.getColumn(1).getString();
    e.amount = list_.getColumn(2).getInt64();
    e.kind = stored_kind(text_of(list_.getColumn(3)));
    e.category = stored_category(text_of(list_.getColumn(4)));
    e.date = parse_stored_date(text_of(list_.getColumn(5)));
    out.push_back(std::move(e));
  }
  return out;
}

std::map<YearMonth, MonthTotals>
EntryRepository::totals_between(YearMonth first, YearMonth last) const {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{totals_};
  totals_.bind(1, first_day_key(first));
  totals_.bind(2, last_day_key(last));

  std::map<YearMonth, MonthTotals> out;
  while (totals_.executeStep()) {
    const auto month = parse_year_month(text_of(totals_.getColumn(0)));
    if (!month)
      continue;
    auto &t = out[*month];
    const Cents sum = totals_.getColumn(2).getInt64();
    if (text_of(totals_.getColumn(1)) == key(Kind::income))
      t.income = sum;
    else
      t.expenses = sum;
    t.count += totals_.getColumn(3).getInt();
  }
  return out;
}

CategoryTotals EntryRepository::expenses_by_category(YearMonth first,
                                                     YearMonth last) const {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{by_category_};
  by_category_.bind(1, first_day_key(first));
  by_category_.bind(2, last_day_key(last));

  CategoryTotals out{};
  while (by_category_.executeStep()) {
    const auto category = stored_category(text_of(by_category_.getColumn(0)));
    out[static_cast<std::size_t>(category)] +=
        by_category_.getColumn(1).getInt64();
  }
  return out;
}

std::vector<YearMonth> EntryRepository::months_with_entries() const {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{months_};

  std::vector<YearMonth> out;
  while (months_.executeStep())
    if (const auto m = parse_year_month(text_of(months_.getColumn(0))))
      out.push_back(*m);
  return out;
}

std::vector<Person> EntryRepository::people() const {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{people_};

  std::vector<Person> out;
  while (people_.executeStep())
    out.push_back({people_.getColumn(0).getInt64(), people_.getColumn(1).getString()});
  return out;
}

std::int64_t EntryRepository::insert_person(std::string_view name) {
  const ResetOnExit reset{add_person_};
  add_person_.bind(1, std::string{name});
  try {
    add_person_.exec();
  } catch (const SQLite::Exception &e) {
    if (is_constraint_violation(e))
      throw ValidationError{"That person already exists."};
    throw;
  }
  return db_.getLastInsertRowid();
}

std::int64_t EntryRepository::add_person(std::string_view name) {
  const std::scoped_lock lock{mutex_};
  return insert_person(name);
}

void EntryRepository::remove_person(std::int64_t id) {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{remove_person_};
  remove_person_.bind(1, id);
  remove_person_.exec();
}

std::map<std::int64_t, int> EntryRepository::share_counts(YearMonth month) const {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{share_counts_};
  share_counts_.bind(1, first_day_key(month));
  share_counts_.bind(2, last_day_key(month));

  std::map<std::int64_t, int> out;
  while (share_counts_.executeStep())
    out[share_counts_.getColumn(0).getInt64()] = share_counts_.getColumn(1).getInt();
  return out;
}

std::vector<Share> EntryRepository::shares(std::int64_t entry_id) const {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{shares_};
  shares_.bind(1, entry_id);

  std::vector<Share> out;
  while (shares_.executeStep())
    out.push_back({shares_.getColumn(0).getInt64(), shares_.getColumn(1).getString(),
                   shares_.getColumn(2).getInt64()});
  return out;
}

void EntryRepository::insert_share(std::int64_t entry_id,
                                   std::int64_t person_id, Cents cents) {
  if (cents <= 0)
    throw ValidationError{"The amount is not valid."};
  {
    const ResetOnExit reset{person_exists_};
    person_exists_.bind(1, person_id);
    if (!person_exists_.executeStep())
      throw ValidationError{"Select a person."};
  }
  {
    const ResetOnExit reset{share_remaining_};
    share_remaining_.bind(1, entry_id);
    if (!share_remaining_.executeStep())
      throw ValidationError{"Entry not found."};
    if (cents > share_remaining_.getColumn(0).getInt64())
      throw ValidationError{"The shared amount exceeds the entry total."};
  }
  const ResetOnExit reset{add_share_};
  add_share_.bind(1, entry_id);
  add_share_.bind(2, person_id);
  add_share_.bind(3, cents);
  try {
    add_share_.exec();
  } catch (const SQLite::Exception &e) {
    // Entry and person exist, so the only constraint left is the primary key.
    if (is_constraint_violation(e))
      throw ValidationError{
          "That person already has a share in this entry. Remove it first."};
    throw;
  }
}

void EntryRepository::add_share(std::int64_t entry_id, std::int64_t person_id,
                                Cents cents) {
  const std::scoped_lock lock{mutex_};
  insert_share(entry_id, person_id, cents);
}

void EntryRepository::add_share_with_new_person(std::int64_t entry_id,
                                                std::string_view name,
                                                Cents cents) {
  const std::scoped_lock lock{mutex_};
  SQLite::Transaction tx{db_}; // rolls back on any exception below
  insert_share(entry_id, insert_person(name), cents);
  tx.commit();
}

void EntryRepository::remove_share(std::int64_t entry_id, std::int64_t person_id) {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{remove_share_};
  remove_share_.bind(1, entry_id);
  remove_share_.bind(2, person_id);
  remove_share_.exec();
}

std::vector<PersonDebt> EntryRepository::debts(YearMonth month) const {
  const std::scoped_lock lock{mutex_};
  const ResetOnExit reset{debts_};
  debts_.bind(1, first_day_key(month));
  debts_.bind(2, last_day_key(month));

  std::vector<PersonDebt> out;
  while (debts_.executeStep())
    out.push_back({debts_.getColumn(0).getInt64(), debts_.getColumn(1).getString(),
                   debts_.getColumn(2).getInt64(), debts_.getColumn(3).getInt64()});
  return out;
}

} // namespace fin
