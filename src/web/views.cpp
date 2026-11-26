#include "web/views.hpp"

#include "core/calendar.hpp"
#include "core/money.hpp"

#include <utility>

namespace fin::web {
namespace {

using Context = crow::mustache::context;
using List = crow::json::wvalue::list;

std::string str(std::string_view s) { return std::string{s}; }

/// Cents -> euros for the Chart.js charts.
double euros(Cents c) { return static_cast<double>(c) / 100.0; }

crow::response html(std::string body) {
  crow::response res{std::move(body)};
  res.set_header("Content-Type", "text/html; charset=utf-8");
  return res;
}

void put_totals(Context &c, const MonthSummary &s) {
  c["label"] = format_month_label(s.month);
  c["income"] = format_money(s.totals.income);
  c["expenses"] = format_money(s.totals.expenses);
  c["balance"] = format_money(s.totals.balance());
  c["count"] = s.totals.count;
}

void put_categories(Context &c, const MonthSummary &s) {
  List tops;
  tops.reserve(s.by_category.size());
  for (const auto &share : s.by_category) {
    Context item;
    item["name"] = str(info(share.category).label);
    item["amount"] = format_money(share.amount);
    item["pct"] = share.percent;
    tops.push_back(std::move(item));
  }
  c["has_tops"] = !tops.empty();
  c["tops"] = std::move(tops);
}

/// Doughnut chart data, serialized for the <script type="application/json"> block.
std::string category_chart_json(const std::vector<CategoryShare> &shares) {
  List labels, values;
  for (const auto &share : shares) {
    labels.push_back(str(info(share.category).label));
    values.push_back(euros(share.amount));
  }
  crow::json::wvalue chart;
  chart["labels"] = std::move(labels);
  chart["values"] = std::move(values);
  return chart.dump();
}

std::string history_chart_json(std::span<const HistoryPoint> history) {
  List labels, income, expenses;
  for (const auto &p : history) {
    labels.push_back(format_month_label(p.month));
    income.push_back(euros(p.totals.income));
    expenses.push_back(euros(p.totals.expenses));
  }
  crow::json::wvalue chart;
  chart["labels"] = std::move(labels);
  chart["income"] = std::move(income);
  chart["expenses"] = std::move(expenses);
  return chart.dump();
}

Context month_content_context(const MonthReport &r,
                              const std::map<std::int64_t, int> &share_counts) {
  Context c;
  put_totals(c, r.summary);
  put_categories(c, r.summary);
  c["chart"] = category_chart_json(r.summary.by_category);

  List rows;
  rows.reserve(r.entries.size());
  for (const auto &e : r.entries) {
    Context row;
    row["id"] = e.id;
    row["title"] = e.title;
    row["amount"] = format_money(e.amount);
    row["kind"] = str(key(e.kind));
    row["kind_label"] = str(label(e.kind));
    row["category"] = str(info(e.category).label);
    row["date"] = format_date(e.date);
    const int count = share_counts.contains(e.id) ? share_counts.at(e.id) : 0;
    row["share_label"] = count == 0 ? "Not shared"
                        : count == 1 ? "1 person"
                                     : std::to_string(count) + " people";
    rows.push_back(std::move(row));
  }
  c["rows"] = std::move(rows);
  return c;
}

std::string people_panel_html(const crow::mustache::template_t &tpl,
                              std::span<const PersonDebt> debts) {
  Context c;
  List list;
  for (const auto &d : debts) {
    Context item;
    item["id"] = d.id;
    item["name"] = d.name;
    item["month_total"] = format_money(d.month_total);
    item["total"] = format_money(d.total);
    list.push_back(std::move(item));
  }
  c["people"] = std::move(list);
  return tpl.render_string(c);
}

Context shares_context(std::int64_t entry_id, std::span<const Share> shares,
                       std::span<const Person> people) {
  Context c;
  c["entry_id"] = entry_id;
  List list;
  for (const auto &s : shares) {
    Context item;
    item["person_id"] = s.person_id;
    item["name"] = s.name;
    item["amount"] = format_money(s.amount);
    list.push_back(std::move(item));
  }
  c["has_shares"] = !list.empty();
  c["shares"] = std::move(list);

  List options;
  for (const auto &p : people) {
    Context item;
    item["id"] = p.id;
    item["name"] = p.name;
    options.push_back(std::move(item));
  }
  c["people"] = std::move(options);
  return c;
}

} // namespace

Views::Views()
    : layout_{crow::mustache::load("layout.html")},
      dashboard_{crow::mustache::load("dashboard.html")},
      month_{crow::mustache::load("month.html")},
      month_content_{crow::mustache::load("month_content.html")},
      people_{crow::mustache::load("people.html")},
      shares_{crow::mustache::load("shares.html")} {}

crow::response Views::layout(std::string title, bool dashboard_active,
                             std::string content,
                             std::span<const YearMonth> months,
                             YearMonth selected, YearMonth current) const {
  Context c;
  c["title"] = std::move(title);
  c["current_month"] = format_month_key(current);
  c["dash_active"] = dashboard_active;
  c["month_active"] = !dashboard_active;

  List options;
  options.reserve(months.size());
  for (const auto m : months) {
    Context item;
    item["month"] = format_month_key(m);
    item["label"] = format_month_label(m);
    item["selected"] = !dashboard_active && m == selected;
    options.push_back(std::move(item));
  }
  c["months"] = std::move(options);
  c["content"] = std::move(content);
  return html(layout_.render_string(c));
}

crow::response Views::dashboard_page(const Dashboard &dashboard,
                                     std::span<const YearMonth> months,
                                     YearMonth current,
                                     std::span<const PersonDebt> debts) const {
  Context c;
  put_totals(c, dashboard.current);
  put_categories(c, dashboard.current);
  c["current_month"] = format_month_key(current);
  c["chart"] = history_chart_json(dashboard.history);
  c["current_chart"] =
      history_chart_json(std::span<const HistoryPoint>{&dashboard.history.back(), 1});
  c["category_chart"] = category_chart_json(dashboard.current.by_category);
  c["history_category_chart"] = category_chart_json(dashboard.history_by_category);
  c["has_history_tops"] = !dashboard.history_by_category.empty();
  c["people_panel"] = people_panel_html(people_, debts);
  return layout("Dashboard", true, dashboard_.render_string(c), months, current,
                current);
}

crow::response Views::month_page(const MonthReport &report,
                                 std::span<const YearMonth> months,
                                 YearMonth current, Date today,
                                 const std::map<std::int64_t, int> &share_counts) const {
  const YearMonth month = report.summary.month;
  const YearMonth prev = month - std::chrono::months{1};
  const YearMonth next = month + std::chrono::months{1};

  Context c;
  put_totals(c, report.summary);
  c["month"] = format_month_key(month);
  c["prev_month"] = format_month_key(prev);
  c["next_month"] = format_month_key(next);
  c["prev_label"] = format_month_label(prev);
  c["next_label"] = format_month_label(next);
  c["today"] = format_date(today);
  c["month_content"] =
      month_content_.render_string(month_content_context(report, share_counts));

  return layout("Expenses for " + format_month_label(month), false,
                month_.render_string(c), months, month, current);
}

crow::response Views::month_fragment(
    const MonthReport &report,
    const std::map<std::int64_t, int> &share_counts) const {
  return html(month_content_.render_string(
      month_content_context(report, share_counts)));
}

crow::response Views::people_panel(std::span<const PersonDebt> debts) const {
  return html(people_panel_html(people_, debts));
}

crow::response Views::shares_dialog(std::int64_t entry_id,
                                    std::span<const Share> shares,
                                    std::span<const Person> people) const {
  return html(shares_.render_string(shares_context(entry_id, shares, people)));
}

} // namespace fin::web
