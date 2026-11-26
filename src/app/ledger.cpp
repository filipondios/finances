#include "app/ledger.hpp"
#include "core/calendar.hpp"
#include "core/errors.hpp"
#include "core/text.hpp"
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

std::vector<Person> Ledger::people() const { return repo_->people(); }

std::int64_t Ledger::add_person(std::string_view raw_name) {
  const auto name = trim(raw_name);
  if (name.empty() || name.size() > 50)
    throw ValidationError{"The name is required (maximum 50 characters)."};
  return repo_->add_person(name);
}

void Ledger::remove_person(std::int64_t id) { repo_->remove_person(id); }

std::map<std::int64_t, int> Ledger::share_counts(YearMonth month) const {
  return repo_->share_counts(month);
}

std::vector<Share> Ledger::shares(std::int64_t entry_id) const {
  return repo_->shares(entry_id);
}

void Ledger::add_share(std::int64_t entry_id, std::int64_t person_id, Cents cents) {
  const auto known = std::ranges::any_of(repo_->people(), [person_id](const Person &p) {
    return p.id == person_id;
  });
  if (!known)
    throw ValidationError{"Select a person."};
  if (!repo_->add_share(entry_id, person_id, cents))
    throw ValidationError{"The shared amount exceeds the entry total."};
}

void Ledger::remove_share(std::int64_t entry_id, std::int64_t person_id) {
  repo_->remove_share(entry_id, person_id);
}

std::vector<PersonDebt> Ledger::debts(YearMonth month) const {
  return repo_->debts(month);
}

} // namespace fin
