#include "app/ledger.hpp"
#include "db/entry_repository.hpp"
#include "web/routes.hpp"
#include "web/views.hpp"

#include <crow.h>

#include <exception>
#include <iostream>

int main() {
  try {
    fin::EntryRepository repository{"finances.db"};
    fin::Ledger ledger{repository};
    const fin::web::Views views;

    crow::SimpleApp app;
    fin::web::register_routes(app, ledger, views);
    app.port(8080).bindaddr("127.0.0.1").multithreaded().run();
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << '\n';
    return 1;
  }
}
