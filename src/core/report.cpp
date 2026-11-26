#include "core/report.hpp"
#include <algorithm>
#include <functional>
#include <numeric>
#include <utility>

namespace fin {

std::vector<CategoryShare> rank_categories(const CategoryTotals &per_category) {
  const Cents total_expenses =
      std::accumulate(per_category.begin(), per_category.end(), Cents{0});
  std::vector<CategoryShare> out;
  out.reserve(per_category.size());
  for (std::size_t i = 0; i < per_category.size(); ++i) {
    if (per_category[i] <= 0)
      continue;
    // Integer half-up rounding: (100 * a / total) + 1/2.
    const auto percent = static_cast<int>(
        (per_category[i] * 200 + total_expenses) / (total_expenses * 2));
    out.push_back({static_cast<Category>(i), per_category[i], percent});
  }
  std::ranges::stable_sort(out, std::greater{}, &CategoryShare::amount);
  return out;
}

MonthReport make_month_report(YearMonth month, std::vector<Entry> entries) {
  MonthTotals totals;
  CategoryTotals per_category{};
  for (const auto &e : entries) {
    if (e.kind == Kind::income) {
      totals.income += e.amount;
    } else {
      totals.expenses += e.amount;
      per_category[static_cast<std::size_t>(e.category)] += e.amount;
    }
  }
  totals.count = static_cast<int>(entries.size());
  return {.summary = {.month = month,
                      .totals = totals,
                      .by_category = rank_categories(per_category)},
          .entries = std::move(entries)};
}

} // namespace fin
