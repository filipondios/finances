#include "web/routes.hpp"

#include "core/calendar.hpp"
#include "core/errors.hpp"
#include "core/money.hpp"

#include <exception>
#include <iostream>
#include <string>

namespace fin::web {
namespace {

/// htmx does not swap 4xx/5xx responses by default, so form errors are
/// returned as 200 and retargeted to the form that made the request.
crow::response form_error(const std::string &message,
                          const char *target = "#form-error") {
  crow::response res{200, message};
  res.set_header("HX-Retarget", target);
  res.set_header("HX-Reswap", "innerHTML");
  return res;
}

std::int64_t to_id(std::string_view s) {
  if (s.empty())
    throw ValidationError{"Invalid id."};
  std::size_t pos = 0;
  long long value = 0;
  try {
    value = std::stoll(std::string{s}, &pos);
  } catch (...) {
    throw ValidationError{"Invalid id."};
  }
  if (pos != s.size() || value <= 0)
    throw ValidationError{"Invalid id."};
  return value;
}

} // namespace

void register_routes(crow::SimpleApp &app, Ledger &ledger, const Views &views) {
  CROW_ROUTE(app, "/")([&] {
    const YearMonth current = month_of(today());
    return views.dashboard_page(ledger.dashboard(current),
                                 ledger.selectable_months(current), current,
                                 ledger.debts(current));
  });

  CROW_ROUTE(app, "/month/<string>")
  ([&](const std::string &raw_month) {
    const auto month = parse_year_month(raw_month);
    if (!month)
      return crow::response{404, "Invalid month"};
    const Date now = today();
    const YearMonth current = month_of(now);
    return views.month_page(ledger.month_report(*month),
                            ledger.selectable_months(current), current, now,
                            ledger.share_counts(*month));
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

          auto res = views.month_fragment(ledger.month_report(month),
                                          ledger.share_counts(month));
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
        return views.month_fragment(ledger.month_report(*month),
                                    ledger.share_counts(*month));
      });

  CROW_ROUTE(app, "/people")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req) {
        try {
          const auto params = req.get_body_params();
          const char *name = params.get("name");
          (void)ledger.add_person(name ? name : "");
          return views.people_panel(ledger.debts(month_of(today())));
        } catch (const ValidationError &e) {
          return form_error(e.what(), "#person-error");
        } catch (const std::exception &e) {
          std::cerr << "POST /people: " << e.what() << '\n';
          return form_error("Could not add the person.", "#person-error");
        }
      });

  CROW_ROUTE(app, "/people/<int>")
      .methods(crow::HTTPMethod::DELETE)([&](int id) {
        ledger.remove_person(id);
        return views.people_panel(ledger.debts(month_of(today())));
      });

  CROW_ROUTE(app, "/entries/<int>/shares")([&](int id) {
    return views.shares_dialog(id, ledger.shares(id), ledger.people());
  });

  CROW_ROUTE(app, "/entries/<int>/shares")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req, int id) {
        try {
          const auto params = req.get_body_params();
          const auto field = [&params](const char *name) -> std::string_view {
            const char *value = params.get(name);
            return value ? value : "";
          };
          const auto selected = field("person_id");
          const std::int64_t person_id = selected == "new"
                                             ? ledger.add_person(field("new_person"))
                                             : to_id(selected);
          ledger.add_share(id, person_id, parse_amount(field("amount")));
          crow::response res{200, ""};
          res.set_header("HX-Refresh", "true");
          return res;
        } catch (const ValidationError &e) {
          return form_error(e.what(), "#share-error");
        } catch (const std::exception &e) {
          std::cerr << "POST /entries/<id>/shares: " << e.what() << '\n';
          return form_error("Could not add the share.", "#share-error");
        }
      });

  CROW_ROUTE(app, "/entries/<int>/shares/<int>")
      .methods(crow::HTTPMethod::DELETE)([&](int entry_id, int person_id) {
        ledger.remove_share(entry_id, person_id);
        crow::response res{200, ""};
        res.set_header("HX-Refresh", "true");
        return res;
      });
}

} // namespace fin::web
