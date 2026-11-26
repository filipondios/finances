#include <algorithm>
#include <cctype>
#include <cmath>
#include <crow.h>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <sqlite3.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

const char *const kCategories[] = {"alimentacion", "transporte", "vivienda",
                                   "ocio", "otros"};
const char *const kCategoryLabels[] = {"Alimentación", "Transporte", "Vivienda",
                                       "Ocio", "Otros"};
const char *const kMonthNames[] = {"Ene", "Feb", "Mar", "Abr", "May", "Jun",
                                   "Jul", "Ago", "Sep", "Oct", "Nov", "Dic"};
sqlite3 *g_db = nullptr;

std::string trim(const std::string &s) {
  size_t b = 0, e = s.size(); // clang-format off
  while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) { ++b; }
  while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) { --e; }
  return s.substr(b, e - b); // clang-format on
}

bool is_digits(const std::string &s, size_t from, size_t count) {
  if (from + count > s.size())
    return false;
  for (size_t i = from; i < from + count; ++i)
    if (!std::isdigit(static_cast<unsigned char>(s[i])))
      return false;
  return true;
}

void exec(const std::string &sql) {
  char *err = nullptr;
  if (sqlite3_exec(g_db, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
    std::string msg = err ? err : sqlite3_errmsg(g_db);
    sqlite3_free(err);
    throw std::runtime_error("SQLite: " + msg);
  }
}

struct Stmt {
  sqlite3_stmt *s = nullptr;

  explicit Stmt(const std::string &sql) {
    if (sqlite3_prepare_v2(g_db, sql.c_str(), -1, &s, nullptr) != SQLITE_OK ||
        !s)
      throw std::runtime_error(std::string("SQLite: ") + sqlite3_errmsg(g_db));
  }
  ~Stmt() {
    if (s)
      sqlite3_finalize(s);
  }
  Stmt(const Stmt &) = delete;
  Stmt &operator=(const Stmt &) = delete;

  bool step() { return sqlite3_step(s) == SQLITE_ROW; }
  void bind(int i, const std::string &v) {
    sqlite3_bind_text(s, i, v.data(), static_cast<int>(v.size()),
                      SQLITE_TRANSIENT);
  }
  void bind(int i, long long v) { sqlite3_bind_int64(s, i, v); }
  std::string text(int i) const {
    const unsigned char *p = sqlite3_column_text(s, i);
    return p ? reinterpret_cast<const char *>(p) : "";
  }
  long long integer(int i) const { return sqlite3_column_int64(s, i); }
};

void init_db() {
  if (sqlite3_open("finances.db", &g_db) != SQLITE_OK)
    throw std::runtime_error(
        std::string("No se puede abrir la base de datos: ") +
        sqlite3_errmsg(g_db));
  exec(R"sql(
        CREATE TABLE IF NOT EXISTS entries (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            title TEXT NOT NULL,
            amount_cents INTEGER NOT NULL CHECK (amount_cents > 0),
            kind TEXT NOT NULL CHECK (kind IN ('expense', 'income')),
            category TEXT NOT NULL CHECK (category IN ('alimentacion', 'transporte', 'vivienda', 'ocio', 'otros')),
            date TEXT NOT NULL,
            created_at TEXT NOT NULL DEFAULT (datetime('now'))
        );
        CREATE INDEX IF NOT EXISTS entries_by_date ON entries(date);
    )sql");
}

std::string money(long long cents) {
  bool neg = cents < 0;
  unsigned long long v = neg ? static_cast<unsigned long long>(-cents)
                             : static_cast<unsigned long long>(cents);
  std::string whole = std::to_string(v / 100);
  for (size_t i = whole.size(); i > 3; i -= 3)
    whole.insert(i - 3, ".");
  std::ostringstream os;
  if (neg)
    os << "-";
  os << whole << "," << std::setw(2) << std::setfill('0') << (v % 100) << " €";
  return os.str();
}

bool valid_month(const std::string &m) {
  return m.size() == 7 && m[4] == '-' && is_digits(m, 0, 4) &&
         is_digits(m, 5, 2) && std::stoi(m.substr(5, 2)) >= 1 &&
         std::stoi(m.substr(5, 2)) <= 12;
}

bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

bool valid_date(const std::string &d) {
  if (d.size() != 10 || d[4] != '-' || d[7] != '-' || !is_digits(d, 0, 4) ||
      !is_digits(d, 5, 2) || !is_digits(d, 8, 2))
    return false;
  int y = std::stoi(d.substr(0, 4)), m = std::stoi(d.substr(5, 2)),
      day = std::stoi(d.substr(8, 2));
  if (m < 1 || m > 12)
    return false;
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int max = days[m - 1] + (m == 2 && leap(y) ? 1 : 0);
  return day >= 1 && day <= max;
}

std::string now_string(const char *fmt) {
  std::time_t t = std::time(nullptr);
  std::tm local{};
  localtime_r(&t, &local);
  char buf[32];
  std::snprintf(buf, sizeof(buf), fmt, local.tm_year + 1900, local.tm_mon + 1,
                local.tm_mday);
  return buf;
}

std::string shift_month(const std::string &m, int delta) {
  long long total = static_cast<long long>(std::stoi(m.substr(0, 4))) * 12 +
                    std::stoi(m.substr(5, 2)) - 1 + delta;
  long long y = total / 12, r = total % 12;
  if (r < 0) {
    r += 12;
    --y;
  }
  char buf[16];
  std::snprintf(buf, sizeof(buf), "%04lld-%02lld", y, r + 1);
  return buf;
}

std::string month_label(const std::string &m) {
  int mm = std::stoi(m.substr(5, 2));
  return std::string(kMonthNames[mm - 1]) + " " + m.substr(0, 4);
}

long long parse_amount(const std::string &raw) {
  std::string s = trim(raw);
  for (char &c : s)
    if (c == ',')
      c = '.';
  if (s.empty())
    throw std::runtime_error("Introduce un importe.");
  size_t pos = 0;
  double value = 0;
  try {
    value = std::stod(s, &pos);
  } catch (...) {
    throw std::runtime_error("El importe no es válido.");
  }
  if (pos != s.size() || !std::isfinite(value) || value <= 0 || value > 1e12)
    throw std::runtime_error("El importe no es válido.");
  long long cents = std::llround(value * 100);
  if (cents <= 0)
    throw std::runtime_error("El importe no es válido.");
  return cents;
}

struct Totals {
  long long income = 0, expenses = 0;
  int count = 0;
  long long balance() const { return income - expenses; }
};

Totals totals_for(const std::string &month) {
  Stmt q("SELECT kind, SUM(amount_cents), COUNT(*) FROM entries WHERE "
         "substr(date,1,7)=? GROUP BY kind");
  q.bind(1, month);
  Totals t;
  while (q.step()) {
    long long sum = q.integer(1);
    t.count += static_cast<int>(q.integer(2));
    if (q.text(0) == "income")
      t.income = sum;
    else
      t.expenses = sum;
  }
  return t;
}

std::vector<std::string> months_available() {
  std::vector<std::string> out{now_string("%04d-%02d")};
  Stmt q("SELECT DISTINCT substr(date,1,7) AS m FROM entries ORDER BY m DESC");
  while (q.step()) {
    std::string m = q.text(0);
    if (std::find(out.begin(), out.end(), m) == out.end())
      out.push_back(m);
  }
  std::sort(out.begin(), out.end(), std::greater<std::string>());
  return out;
}

std::string category_label(const std::string &category) {
  for (size_t i = 0; i < 5; ++i)
    if (category == kCategories[i])
      return kCategoryLabels[i];
  return "Otros";
}

void fill_month(crow::json::wvalue &c, const std::string &month) {
  Totals t = totals_for(month);
  c["label"] = month_label(month);
  c["month"] = month;
  std::string prev = shift_month(month, -1), next = shift_month(month, 1);
  c["prev_month"] = prev;
  c["next_month"] = next;
  c["prev_label"] = month_label(prev);
  c["next_label"] = month_label(next);
  c["income"] = money(t.income);
  c["expenses"] = money(t.expenses);
  c["balance"] = money(t.balance());
  c["count"] = t.count;
  c["today"] = now_string("%04d-%02d-%02d");

  crow::json::wvalue::list rows;
  Stmt q("SELECT id, title, amount_cents, kind, category, date, created_at "
         "FROM entries WHERE substr(date,1,7)=? ORDER BY created_at DESC, id "
         "DESC");
  q.bind(1, month);
  while (q.step()) {
    crow::json::wvalue r;
    std::string kind = q.text(3);
    r["title"] = q.text(1);
    r["amount"] = money(q.integer(2));
    r["kind"] = kind;
    r["kind_label"] = kind == "income" ? "Ingreso" : "Gasto";
    r["category"] = category_label(q.text(4));
    r["date"] = q.text(5);
    r["added"] = q.text(6).substr(0, 16);
    rows.push_back(std::move(r));
  }
  c["rows"] = std::move(rows);

  crow::json::wvalue::list labels, values, tops;
  Stmt cs("SELECT category, SUM(amount_cents) AS total FROM entries "
          "WHERE substr(date,1,7)=? AND kind='expense' GROUP BY category ORDER "
          "BY total DESC");
  cs.bind(1, month);
  while (cs.step()) {
    labels.push_back(category_label(cs.text(0)));
    values.push_back(cs.integer(1) / 100.0);
    crow::json::wvalue item;
    item["name"] = category_label(cs.text(0));
    item["amount"] = money(cs.integer(1));
    item["pct"] =
        t.expenses > 0
            ? static_cast<int>(std::llround(cs.integer(1) * 100.0 / t.expenses))
            : 0;
    tops.push_back(std::move(item));
  }
  c["has_tops"] = !tops.empty();
  c["tops"] = std::move(tops);

  crow::json::wvalue chart;
  chart["labels"] = std::move(labels);
  chart["values"] = std::move(values);
  c["chart"] = chart.dump();
}

} // namespace

int main() {
  init_db();

  auto layout = crow::mustache::load("layout.html");
  auto dashboard = crow::mustache::load("dashboard.html");
  auto month_tpl = crow::mustache::load("month.html");

  crow::SimpleApp app;

  auto base = [](const std::string &title, const std::string &active) {
    crow::mustache::context c;
    c["title"] = title;
    c["current_month"] = now_string("%04d-%02d");
    bool is_dashboard = active == "dashboard";
    c["dash_active"] = is_dashboard;
    c["month_active"] = !is_dashboard;
    crow::json::wvalue::list months;
    for (const auto &m : months_available()) {
      crow::json::wvalue item;
      item["month"] = m;
      item["label"] = month_label(m);
      item["selected"] = m == active;
      months.push_back(std::move(item));
    }
    c["months"] = std::move(months);
    return c;
  };

  auto page = [&](const crow::mustache::context &c, const std::string &inner) {
    crow::mustache::context full = c;
    full["content"] = inner;
    crow::response res(layout.render_string(full));
    res.set_header("Content-Type", "text/html; charset=utf-8");
    return res;
  };

  CROW_ROUTE(app, "/")([&]() {
    std::string cm = now_string("%04d-%02d");
    auto c = base("Dashboard", "dashboard");
    fill_month(c, cm);

    crow::json::wvalue::list labels, income, expenses;
    for (int i = 5; i >= 0; --i) {
      std::string m = shift_month(cm, -i);
      Totals t = totals_for(m);
      labels.push_back(month_label(m));
      income.push_back(t.income / 100.0);
      expenses.push_back(t.expenses / 100.0);
    }
    crow::json::wvalue chart;
    chart["labels"] = std::move(labels);
    chart["income"] = std::move(income);
    chart["expenses"] = std::move(expenses);
    c["chart"] = chart.dump();
    return page(c, dashboard.render_string(c));
  });

  auto month_page = [&](const crow::request &, std::string m) {
    if (!valid_month(m))
      return crow::response(404, "Mes no válido");
    auto c = base("Gastos de " + month_label(m), m);
    fill_month(c, m);
    return page(c, month_tpl.render_string(c));
  };
  CROW_ROUTE(app, "/month/<string>")(month_page);

  CROW_ROUTE(app, "/entries")
      .methods(crow::HTTPMethod::POST)([&](const crow::request &req) {
        try {
          auto p = req.get_body_params();
          auto field = [&](const char *key) {
            const char *v = p.get(key);
            return v ? trim(v) : std::string();
          };
          std::string title = field("title");
          std::string kind = field("kind");
          std::string category = field("category");
          std::string date = field("date");
          std::string month = field("month");
          long long cents = parse_amount(field("amount"));

          if (title.empty() || title.size() > 100)
            throw std::runtime_error(
                "El título es obligatorio (máximo 100 caracteres).");
          if (kind != "expense" && kind != "income")
            throw std::runtime_error("Selecciona gasto o ingreso.");
          bool known_category = false;
          for (const char *k : kCategories)
            if (category == k)
              known_category = true;
          if (!known_category)
            throw std::runtime_error("Selecciona una categoría.");
          if (!valid_month(month) || !valid_date(date) ||
              date.substr(0, 7) != month)
            throw std::runtime_error(
                "La fecha debe pertenecer al mes mostrado.");

          Stmt ins("INSERT INTO entries (title, amount_cents, kind, category, "
                   "date) VALUES (?, ?, ?, ?, ?)");
          ins.bind(1, title);
          ins.bind(2, cents);
          ins.bind(3, kind);
          ins.bind(4, category);
          ins.bind(5, date);
          if (sqlite3_step(ins.s) != SQLITE_DONE)
            throw std::runtime_error(std::string("No se pudo guardar: ") +
                                     sqlite3_errmsg(g_db));

          auto c = base("Gastos de " + month_label(month), month);
          fill_month(c, month);
          crow::response res(month_tpl.render_string(c));
          res.set_header("Content-Type", "text/html; charset=utf-8");
          res.set_header("HX-Trigger", "entry-added");
          return res;
        } catch (const std::exception &e) {
          // 200 + HX-Retarget: htmx no intercambia respuestas 4xx por defecto.
          crow::response res(200, e.what());
          res.set_header("HX-Retarget", "#form-error");
          return res;
        }
      });

  app.port(8080).bindaddr("127.0.0.1").multithreaded().run();
}
