#include "web/routes.hpp"

#include "core/calendar.hpp"
#include "core/errors.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace fin::web {
namespace {

/// htmx does not swap 4xx/5xx responses by default, so form errors are
/// returned as 200 and retargeted to `#form-error`.
crow::response form_error(const std::string &message) {
  crow::response res{200, message};
  res.set_header("HX-Retarget", "#form-error");
  return res;
}

} // namespace

void register_routes(crow::SimpleApp &app, Ledger &ledger, const Views &views) {
  CROW_ROUTE(app, "/")([&] {
    const YearMonth current = month_of(today());
    return views.dashboard_page(ledger.dashboard(current),
                                ledger.selectable_months(current), current);
  });

  CROW_ROUTE(app, "/month/<string>")
  ([&](const std::string &raw_month) {
    const auto month = parse_year_month(raw_month);
    if (!month)
      return crow::response{404, "Invalid month"};
    const Date now = today();
    const YearMonth current = month_of(now);
    return views.month_page(ledger.month_report(*month),
                            ledger.selectable_months(current), current, now);
  });

  CROW_ROUTE(app, "/entries")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req) {
        try {
          const auto params = req.get_body_params();
          const auto field = [&params](const char *name) -> std::string_view {
            const char *value = params.get(name);
            return value ? value : "";
          };
          const YearMonth month = ledger.add_entry({.title = field("title"),
                                                    .amount = field("amount"),
                                                    .kind = field("kind"),
                                                    .category = field("category"),
                                                    .date = field("date"),
                                                    .month = field("month")});

          auto res = views.month_fragment(ledger.month_report(month));
          res.set_header("HX-Trigger", "entry-added");
          return res;
        } catch (const ValidationError &e) {
          return form_error(e.what());
        } catch (const std::exception &e) {
          std::cerr << "POST /entries: " << e.what() << '\n';
          return form_error("Could not save the transaction.");
        }
      });

  CROW_ROUTE(app, "/entries/<int>")
      .methods(crow::HTTPMethod::DELETE)([&](int id) {
        const auto month = ledger.remove_entry(id);
        if (!month)
          return crow::response{404, "Entry not found"};
        return views.month_fragment(ledger.month_report(*month));
      });
}

} // namespace fin::web
