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

  /// Entries for a month, most recently inserted first.
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

private:
  mutable std::mutex mutex_;
  mutable SQLite::Database db_;
  mutable SQLite::Statement insert_;
  mutable SQLite::Statement remove_;
  mutable SQLite::Statement list_;
  mutable SQLite::Statement totals_;
  mutable SQLite::Statement by_category_;
  mutable SQLite::Statement months_;
};

} // namespace fin
