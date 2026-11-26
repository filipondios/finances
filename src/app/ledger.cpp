#include "app/ledger.hpp"
#include "core/calendar.hpp"
#include <algorithm>
#include <functional>

namespace fin {

YearMonth Ledger::add_entry(const EntryForm &form) {
  const NewEntry entry = validate(form);
  repo_->add(entry);
  return month_of(entry.date);
}

std::optional<YearMonth> Ledger::remove_entry(std::int64_t id) {
  return repo_->remove(id);
}

MonthReport Ledger::month_report(YearMonth month) const {
  return make_month_report(month, repo_->entries_in(month));
}

Dashboard Ledger::dashboard(YearMonth current) const {
  using std::chrono::months;
  const YearMonth first = current - months{kHistoryMonths - 1};
  const auto totals = repo_->totals_between(first, current);

  Dashboard out;
  out.history.reserve(kHistoryMonths);
  for (YearMonth m = first; m <= current; m += months{1}) {
    const auto it = totals.find(m);
    out.history.push_back({m, it != totals.end() ? it->second : MonthTotals{}});
  }

  const MonthTotals &now = out.history.back().totals;
  // clang-format off
  out.current = {.month = current, .totals = now, 
    .by_category = rank_categories(repo_->expenses_by_category(current, current))};
  out.history_by_category =
      rank_categories(repo_->expenses_by_category(first, current));
  return out;
}

std::vector<YearMonth> Ledger::selectable_months(YearMonth current) const {
  auto months = repo_->months_with_entries();
  months.push_back(current);
  std::ranges::sort(months, std::greater{});
  const auto dup = std::ranges::unique(months);
  months.erase(dup.begin(), dup.end());
  return months;
}

} // namespace fin
