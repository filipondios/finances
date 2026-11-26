#pragma once

#include "core/report.hpp"
#include "core/types.hpp"

#include <crow.h>

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
                 YearMonth current) const;

  [[nodiscard]] crow::response
  month_page(const MonthReport &report, std::span<const YearMonth> months,
             YearMonth current, Date today) const;

  /// Only the contents of `#month-content`: what HTMX swaps in after
  /// adding a transaction.
  [[nodiscard]] crow::response month_fragment(const MonthReport &report) const;

private:
  [[nodiscard]] crow::response
  layout(std::string title, bool dashboard_active, std::string content,
         std::span<const YearMonth> months, YearMonth selected,
         YearMonth current) const;

  crow::mustache::template_t layout_;
  crow::mustache::template_t dashboard_;
  crow::mustache::template_t month_;
  crow::mustache::template_t month_content_;
};

} // namespace fin::web
