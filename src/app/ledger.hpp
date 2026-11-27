#pragma once

#include "core/calendar.hpp"
#include "core/entry_form.hpp"
#include "core/report.hpp"
#include "core/types.hpp"
#include "db/entry_repository.hpp"
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace fin {

class Ledger {
public:
  using Clock = std::function<Date()>;
  static constexpr int kHistoryMonths = 6;

  /// `repo` must outlive the ledger.
  explicit Ledger(EntryRepository &repo, Clock clock = &fin::today)
      : repo_{repo}, clock_{std::move(clock)} {}

  /// Today's date according to the clock.
  [[nodiscard]] Date today() const { return clock_(); }

  /// Validates and saves an entry; returns the month it belongs to.
  /// Throws `ValidationError` if the form is not valid.
  YearMonth add_entry(const EntryForm &form);

  /// Deletes an entry and returns the month it belonged to.
  [[nodiscard]] std::optional<YearMonth> remove_entry(std::int64_t id);

  /// Entries for a month with their totals and category breakdown.
  [[nodiscard]] MonthReport month_report(YearMonth month) const;

  /// Summary of the current month and the trend over the last `kHistoryMonths`.
  [[nodiscard]] Dashboard dashboard(YearMonth current) const;

  /// Months offered in the selector: those with data plus the current one,
  /// newest first and without duplicates.
  [[nodiscard]] std::vector<YearMonth>
  selectable_months(YearMonth current) const;

  [[nodiscard]] std::vector<Person> people() const;
  [[nodiscard]] std::int64_t add_person(std::string_view name);
  void remove_person(std::int64_t id);

  [[nodiscard]] std::map<std::int64_t, int> share_counts(YearMonth month) const;
  [[nodiscard]] std::vector<Share> shares(std::int64_t entry_id) const;

  void add_share(std::int64_t entry_id, std::int64_t person_id, Cents cents);
  void add_share_with_new_person(std::int64_t entry_id, std::string_view name,
                                 Cents cents);
  void remove_share(std::int64_t entry_id, std::int64_t person_id);
  [[nodiscard]] std::vector<PersonDebt> debts(YearMonth month) const;

private:
  EntryRepository &repo_;
  Clock clock_;
};

} // namespace fin
