#pragma once

#include "core/types.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <filesystem>
#include <map>
#include <mutex>
#include <vector>

namespace fin {

/// Access to the `entries` table.
/// Keeps a single connection and prepares statements once; a mutex serializes
/// access so it can be used from Crow's threads. More than enough for a local
/// single-user app, and it avoids preparing SQL on every request.
class EntryRepository {
public:
  /// Opens (or creates) the database and applies the schema.
  explicit EntryRepository(const std::filesystem::path &file);

  EntryRepository(const EntryRepository &) = delete;
  EntryRepository &operator=(const EntryRepository &) = delete;
  void add(const NewEntry &entry);

  /// Deletes an entry and returns its month, or nullopt if it does not exist.
  [[nodiscard]] std::optional<YearMonth> remove(std::int64_t id);

  /// Entries for a month, newest date first (ties: most recently inserted).
  [[nodiscard]] std::vector<Entry> entries_in(YearMonth month) const;

  /// Totals per month in [first, last], in a single query. Months without
  /// entries do not appear in the map.
  [[nodiscard]] std::map<YearMonth, MonthTotals>
  totals_between(YearMonth first, YearMonth last) const;

  /// Expenses in a date range by category.
  [[nodiscard]] CategoryTotals expenses_by_category(YearMonth first,
                                                    YearMonth last) const;

  /// Months (newest first) with at least one entry.
  [[nodiscard]] std::vector<YearMonth> months_with_entries() const;

  [[nodiscard]] std::vector<Person> people() const;
  /// Throws `ValidationError` if the name is already taken.
  [[nodiscard]] std::int64_t add_person(std::string_view name);

  /// Deletes a person and, by cascade, their shares.
  void remove_person(std::int64_t id);

  /// People sharing each entry of a month, keyed by entry id.
  [[nodiscard]] std::map<std::int64_t, int> share_counts(YearMonth month) const;
  [[nodiscard]] std::vector<Share> shares(std::int64_t entry_id) const;

  /// Throws `ValidationError` if the person/entry is missing, the person
  /// already has a share in the entry, or the entry total would be exceeded.
  void add_share(std::int64_t entry_id, std::int64_t person_id, Cents cents);

  /// Creates the person and their share atomically: if the share is
  /// rejected, the person is not created either.
  void add_share_with_new_person(std::int64_t entry_id, std::string_view name,
                                 Cents cents);
  void remove_share(std::int64_t entry_id, std::int64_t person_id);

  /// Amount owed by each person, overall and in `month`.
  [[nodiscard]] std::vector<PersonDebt> debts(YearMonth month) const;

private:
  // Require `mutex_` to be held.
  std::int64_t insert_person(std::string_view name);
  void insert_share(std::int64_t entry_id, std::int64_t person_id, Cents cents);

  mutable std::mutex mutex_;
  mutable SQLite::Database db_;
  mutable SQLite::Statement insert_;
  mutable SQLite::Statement remove_;
  mutable SQLite::Statement list_;
  mutable SQLite::Statement totals_;
  mutable SQLite::Statement by_category_;
  mutable SQLite::Statement months_;
  mutable SQLite::Statement people_;
  mutable SQLite::Statement add_person_;
  mutable SQLite::Statement remove_person_;
  mutable SQLite::Statement share_counts_;
  mutable SQLite::Statement shares_;
  mutable SQLite::Statement share_remaining_;
  mutable SQLite::Statement person_exists_;
  mutable SQLite::Statement add_share_;
  mutable SQLite::Statement remove_share_;
  mutable SQLite::Statement debts_;
};

} // namespace fin
