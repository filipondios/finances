#pragma once

#include "core/entry_form.hpp"
#include "core/report.hpp"
#include "core/types.hpp"
#include "db/entry_repository.hpp"
#include <vector>

namespace fin {

/// Application use cases. Knows nothing about HTTP or templates.
class Ledger {
public:
  static constexpr int kHistoryMonths = 6;
  explicit Ledger(EntryRepository &repo) noexcept : repo_{&repo} {}

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

private:
  EntryRepository *repo_;
};

} // namespace fin
