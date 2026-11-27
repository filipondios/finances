#include "app/ledger.hpp"
#include "db/entry_repository.hpp"
#include "web/guard.hpp"
#include "web/routes.hpp"
#include "web/views.hpp"

#include <crow.h>

#include <array>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

namespace fs = std::filesystem;

constexpr std::array<std::string_view, 8> kTemplates{
    "layout.html", "dashboard.html", "month.html", "month_content.html",
    "people.html", "shares.html", "edit_entry.html", "edit_person.html"};

/// FINANCES_PORT, default 8080.
std::uint16_t port_from_env() {
  const char *raw = std::getenv("FINANCES_PORT");
  if (!raw || !*raw)
    return 8080;
  const std::string_view s{raw};
  unsigned value = 0;
  const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
  if (ec != std::errc{} || end != s.data() + s.size() || value == 0 ||
      value > 65535)
    throw std::runtime_error{"FINANCES_PORT must be a number in 1-65535."};
  return static_cast<std::uint16_t>(value);
}

/// Templates, static files and finances.db live in the working directory.
/// FINANCES_HOME switches to another one; a missing template fails fast
/// (Crow would otherwise serve blank pages and we would create an empty DB).
void enter_home() {
  if (const char *home = std::getenv("FINANCES_HOME"); home && *home)
    fs::current_path(home);
  for (const auto name : kTemplates)
    if (!fs::exists(fs::path{"templates"} / name))
      throw std::runtime_error{"Missing templates/" + std::string{name} +
                               " in " + fs::current_path().string() +
                               ". Run from the project root or set FINANCES_HOME."};
}

} // namespace

int main() {
  try {
    const std::uint16_t port = port_from_env();
    enter_home();

    fin::EntryRepository repository{"finances.db"};
    fin::Ledger ledger{repository};
    const fin::web::Views views;

    fin::web::App app;
    app.get_middleware<fin::web::LocalGuard>().port = port;
    fin::web::register_routes(app, ledger, views);
    app.port(port).bindaddr("127.0.0.1").concurrency(2).run();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}
