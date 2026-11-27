#pragma once

#include "core/report.hpp"
#include "core/types.hpp"

#include <crow.h>

#include <map>
#include <span>
#include <string>

namespace fin::web {

/// Renders the application models with Mustache templates.
/// Immutable after construction, so it can be shared across threads.
class Views {
public:
  /// Loads templates from `templates/` (relative to the current directory).
  Views();

  [[nodiscard]] crow::response
  dashboard_page(const Dashboard &dashboard, std::span<const YearMonth> months,
                 YearMonth current, std::span<const PersonDebt> debts) const;

  [[nodiscard]] crow::response
  month_page(const MonthReport &report, std::span<const YearMonth> months,
             YearMonth current, Date today,
             const std::map<std::int64_t, int> &share_counts) const;

  /// Only the contents of `#month-content`: what HTMX swaps in after
  /// adding a transaction.
  [[nodiscard]] crow::response
  month_fragment(const MonthReport &report,
                 const std::map<std::int64_t, int> &share_counts) const;

  [[nodiscard]] crow::response people_panel(std::span<const PersonDebt> debts) const;

  [[nodiscard]] crow::response shares_dialog(std::int64_t entry_id,
                                             std::span<const Share> shares,
                                             std::span<const Person> people) const;

  [[nodiscard]] crow::response edit_entry_dialog(const Entry &entry) const;
  [[nodiscard]] crow::response edit_person_dialog(const Person &person) const;

private:
  [[nodiscard]] crow::response
  layout(std::string title, bool dashboard_active, std::string content,
         std::span<const YearMonth> months, YearMonth selected,
         YearMonth current) const;

  crow::mustache::template_t layout_;
  crow::mustache::template_t dashboard_;
  crow::mustache::template_t month_;
  crow::mustache::template_t month_content_;
  crow::mustache::template_t people_;
  crow::mustache::template_t shares_;
  crow::mustache::template_t edit_entry_;
  crow::mustache::template_t edit_person_;
};

} // namespace fin::web
