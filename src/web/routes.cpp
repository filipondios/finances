#include "web/routes.hpp"

#include "core/calendar.hpp"
#include "core/errors.hpp"
#include "core/money.hpp"

#include <charconv>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string>
#include <string_view>
#include <system_error>

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

/// Strict positive integer: no sign, spaces or trailing text.
std::int64_t to_id(std::string_view s) {
  std::int64_t value = 0;
  const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || end != s.data() + s.size() || value <= 0)
    throw ValidationError{"Invalid id."};
  return value;
}

/// URL-encoded body of a request. Missing fields read as "". The returned
/// views point into this object, so it must outlive their use.
class Form {
public:
  explicit Form(const crow::request &req) : params_{req.get_body_params()} {}
  Form(const Form &) = delete;
  Form &operator=(const Form &) = delete;

  [[nodiscard]] std::string_view operator[](const char *name) const {
    const char *value = params_.get(name);
    return value ? value : "";
  }

private:
  crow::query_string params_;
};

/// Runs a form handler. A `ValidationError` is shown to the user; any other
/// failure is logged and replaced by `fallback`. Both go to the htmx `target`.
/// (Routes without a form need no wrapper: Crow turns their exceptions into
/// a logged 500.)
template <class Handler>
crow::response guarded(std::string_view route, const char *target,
                       const char *fallback, Handler &&handler) {
  try {
    return handler();
  } catch (const ValidationError &e) {
    return form_error(e.what(), target);
  } catch (const std::exception &e) {
    std::cerr << route << ": " << e.what() << '\n';
    return form_error(fallback, target);
  }
}

} // namespace

void register_routes(App &app, Ledger &ledger, const Views &views) {
  CROW_ROUTE(app, "/")([&] {
    const YearMonth current = month_of(ledger.today());
    return views.dashboard_page(ledger.dashboard(current),
                                 ledger.selectable_months(current), current,
                                 ledger.debts(current));
  });

  CROW_ROUTE(app, "/month/<string>")
  ([&](const std::string &raw_month) {
    const auto month = parse_year_month(raw_month);
    if (!month)
      return crow::response{404, "Invalid month"};
    const Date now = ledger.today();
    const YearMonth current = month_of(now);
    return views.month_page(ledger.month_report(*month),
                            ledger.selectable_months(current), current, now,
                            ledger.share_counts(*month));
  });

  CROW_ROUTE(app, "/entries")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req) {
        return guarded("POST /entries", "#form-error",
                       "Could not save the transaction.", [&] {
          const Form form{req};
          const YearMonth month = ledger.add_entry({.title = form["title"],
                                                    .amount = form["amount"],
                                                    .kind = form["kind"],
                                                    .category = form["category"],
                                                    .date = form["date"],
                                                    .month = form["month"]});

          auto res = views.month_fragment(ledger.month_report(month),
                                          ledger.share_counts(month));
          res.set_header("HX-Trigger", "entry-added");
          return res;
        });
      });

  CROW_ROUTE(app, "/entries/<int>")
      .methods(crow::HTTPMethod::DELETE)([&](int id) {
        const auto month = ledger.remove_entry(id);
        if (!month)
          return crow::response{404, "Entry not found"};
        return views.month_fragment(ledger.month_report(*month),
                                    ledger.share_counts(*month));
      });

  CROW_ROUTE(app, "/entries/<int>/edit")([&](int id) {
    const auto entry = ledger.entry(id);
    if (!entry)
      return crow::response{404, "Entry not found"};
    return views.edit_entry_dialog(*entry);
  });

  CROW_ROUTE(app, "/entries/<int>/edit")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req, int id) {
        return guarded("POST /entries/<id>/edit", "#edit-error",
                       "Could not save the entry.", [&] {
          const Form form{req};
          ledger.update_entry(id, form["title"], form["date"]);
          crow::response res{200, ""};
          res.set_header("HX-Refresh", "true");
          return res;
        });
      });

  CROW_ROUTE(app, "/people")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req) {
        return guarded("POST /people", "#person-error",
                       "Could not add the person.", [&] {
          (void)ledger.add_person(Form{req}["name"]);
          return views.people_panel(ledger.debts(month_of(ledger.today())));
        });
      });

  CROW_ROUTE(app, "/people/<int>")
      .methods(crow::HTTPMethod::DELETE)([&](int id) {
        ledger.remove_person(id);
        return views.people_panel(ledger.debts(month_of(ledger.today())));
      });

  CROW_ROUTE(app, "/people/<int>/edit")([&](int id) {
    const auto person = ledger.person(id);
    if (!person)
      return crow::response{404, "Person not found"};
    return views.edit_person_dialog(*person);
  });

  CROW_ROUTE(app, "/people/<int>/edit")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req, int id) {
        return guarded("POST /people/<id>/edit", "#edit-error",
                       "Could not save the person.", [&] {
          const Form form{req};
          ledger.rename_person(id, form["name"]);
          crow::response res{200, ""};
          res.set_header("HX-Refresh", "true");
          return res;
        });
      });

  CROW_ROUTE(app, "/entries/<int>/shares")([&](int id) {
    return views.shares_dialog(id, ledger.shares(id), ledger.people());
  });

  CROW_ROUTE(app, "/entries/<int>/shares")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req, int id) {
        return guarded("POST /entries/<id>/shares", "#share-error",
                       "Could not add the share.", [&] {
          const Form form{req};
          const auto selected = form["person_id"];
          const Cents cents = parse_amount(form["amount"]);
          if (selected == "new") {
            ledger.add_share_with_new_person(id, form["new_person"], cents);
          } else {
            if (selected.empty())
              throw ValidationError{"Select a person."};
            ledger.add_share(id, to_id(selected), cents);
          }
          crow::response res{200, ""};
          res.set_header("HX-Refresh", "true");
          return res;
        });
      });

  CROW_ROUTE(app, "/entries/<int>/shares/<int>")
      .methods(crow::HTTPMethod::DELETE)([&](int entry_id, int person_id) {
        ledger.remove_share(entry_id, person_id);
        crow::response res{200, ""};
        res.set_header("HX-Refresh", "true");
        return res;
      });

  CROW_ROUTE(app, "/entries/<int>/settlements")([&](int id) {
    return views.settlements_dialog(id, ledger.settlements(id), ledger.people());
  });

  CROW_ROUTE(app, "/entries/<int>/settlements")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req, int id) {
        return guarded("POST /entries/<id>/settlements", "#settle-error",
                       "Could not save the payment.", [&] {
          const Form form{req};
          const auto selected = form["person_id"];
          if (selected.empty())
            throw ValidationError{"Select a person."};
          ledger.add_settlement(id, to_id(selected), parse_amount(form["amount"]));
          crow::response res{200, ""};
          res.set_header("HX-Refresh", "true");
          return res;
        });
      });

  CROW_ROUTE(app, "/entries/<int>/settlements/<int>")
      .methods(crow::HTTPMethod::DELETE)([&](int, int payment_id) {
        ledger.remove_settlement(payment_id);
        crow::response res{200, ""};
        res.set_header("HX-Refresh", "true");
        return res;
      });
}

} // namespace fin::web
