#include "app/ledger.hpp"
#include "core/calendar.hpp"
#include "core/errors.hpp"
#include "core/text.hpp"
#include <algorithm>
#include <functional>

namespace fin {
namespace {
std::string_view valid_text(std::string_view raw, std::size_t max,
                            std::string_view what) {
  const auto text = trim(raw);
  const auto length = text_length(text);
  if (!length)
    throw ValidationError{"The " + std::string{what} +
                          " contains invalid characters."};
  if (*length == 0 || *length > max)
    throw ValidationError{"The " + std::string{what} +
                          " is required (maximum " + std::to_string(max) +
                          " characters)."};
  return text;
}

std::string_view valid_person_name(std::string_view raw) {
  return valid_text(raw, 50, "name");
}
} // namespace

YearMonth Ledger::add_entry(const EntryForm &form) {
  const NewEntry entry = validate(form);
  repo_.add(entry);
  return month_of(entry.date);
}

std::optional<YearMonth> Ledger::remove_entry(std::int64_t id) {
  return repo_.remove(id);
}

std::optional<Entry> Ledger::entry(std::int64_t id) const {
  return repo_.entry(id);
}

void Ledger::update_entry(std::int64_t id, std::string_view raw_title,
                          std::string_view raw_date) {
  const auto title = valid_text(raw_title, kMaxTitleChars, "title");
  const auto date = parse_date(trim(raw_date));
  if (!date)
    throw ValidationError{"The date is not valid."};
  if (!repo_.update_entry(id, title, format_date(*date)))
    throw ValidationError{"Entry not found."};
}

MonthReport Ledger::month_report(YearMonth month) const {
  return make_month_report(month, repo_.entries_in(month));
}

Dashboard Ledger::dashboard(YearMonth current) const {
  using std::chrono::months;
  const YearMonth first = current - months{kHistoryMonths - 1};
  const auto totals = repo_.totals_between(first, current);

  Dashboard out;
  out.history.reserve(kHistoryMonths);
  for (YearMonth m = first; m <= current; m += months{1}) {
    const auto it = totals.find(m);
    out.history.push_back({m, it != totals.end() ? it->second : MonthTotals{}});
  }

  const MonthTotals &now = out.history.back().totals;
  out.current = {.month = current,
                 .totals = now,
                 .by_category = rank_categories(
                     repo_.expenses_by_category(current, current))};
  out.history_by_category =
      rank_categories(repo_.expenses_by_category(first, current));
  return out;
}

std::vector<YearMonth> Ledger::selectable_months(YearMonth current) const {
  auto months = repo_.months_with_entries();
  months.push_back(current);
  std::ranges::sort(months, std::greater{});
  const auto dup = std::ranges::unique(months);
  months.erase(dup.begin(), dup.end());
  return months;
}

std::vector<Person> Ledger::people() const { return repo_.people(); }

std::optional<Person> Ledger::person(std::int64_t id) const {
  return repo_.person(id);
}

std::int64_t Ledger::add_person(std::string_view raw_name) {
  return repo_.add_person(valid_person_name(raw_name));
}

void Ledger::rename_person(std::int64_t id, std::string_view raw_name) {
  const auto name = valid_person_name(raw_name);
  const auto current = repo_.person(id);
  if (!current)
    throw ValidationError{"Person not found."};
  if (current->name == name)
    return;
  repo_.rename_person(id, name);
}

void Ledger::remove_person(std::int64_t id) { repo_.remove_person(id); }

std::map<std::int64_t, int> Ledger::share_counts(YearMonth month) const {
  return repo_.share_counts(month);
}

std::vector<Share> Ledger::shares(std::int64_t entry_id) const {
  return repo_.shares(entry_id);
}

void Ledger::add_share(std::int64_t entry_id, std::int64_t person_id,
                       Cents cents) {
  repo_.add_share(entry_id, person_id, cents);
}

void Ledger::add_share_with_new_person(std::int64_t entry_id,
                                       std::string_view raw_name, Cents cents) {
  repo_.add_share_with_new_person(entry_id, valid_person_name(raw_name), cents);
}

void Ledger::remove_share(std::int64_t entry_id, std::int64_t person_id) {
  repo_.remove_share(entry_id, person_id);
}

std::vector<PersonDebt> Ledger::debts(YearMonth month) const {
  return repo_.debts(month);
}

} // namespace fin
