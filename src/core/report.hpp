#pragma once

#include "core/types.hpp"
#include <vector>

namespace fin {

struct CategoryShare {
  Category category{};
  Cents amount{};
  int percent{};
};

struct MonthSummary {
  YearMonth month{};
  MonthTotals totals;
  std::vector<CategoryShare> by_category;
};

struct MonthReport {
  MonthSummary summary;
  std::vector<Entry> entries;
};

struct HistoryPoint {
  YearMonth month{};
  MonthTotals totals;
};

struct Dashboard {
  MonthSummary current;
  std::vector<HistoryPoint> history;
  std::vector<CategoryShare> history_by_category;
};

/// Sorts categories by descending spend (ties broken by enum order, so the
/// result is deterministic) and computes their percentages over the total
/// expenses in `per_category`.
[[nodiscard]] std::vector<CategoryShare>
rank_categories(const CategoryTotals &per_category);

/// Computes totals and the category breakdown in a single pass over the
/// entries: one database query is enough for the whole page.
[[nodiscard]] MonthReport make_month_report(YearMonth month,
                                            std::vector<Entry> entries);

} // namespace fin
