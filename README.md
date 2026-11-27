# Finances

Local app to record expenses and income and view them by month. C++20 with Crow, SQLite (via SQLiteCpp) and HTML + HTMX/Chart.js.

## Getting started

You need CMake (≥ 3.23), Conan 2 and a compiler with C++20 (`std::format` and the `std::chrono` calendar: GCC ≥ 13, Clang ≥ 17 with libstdc++ 13, MSVC 2022). Crow, Asio and SQLiteCpp (with SQLite3) are managed with Conan; HTMX and Chart.js are served as local static files.

```sh
conan profile detect # first time only
conan install . --output-folder=build --build=missing
conan install . --output-folder=build --build=missing -s build_type=Debug # only if you use the debug preset
cmake --preset release
cmake --build --preset release
./build/release/finances
```

Open <http://127.0.0.1:8080>. The executable reads `templates/` and `static/` and creates `finances.db` in the current working directory, so run it from here (it refuses to start if the templates are missing) or set `FINANCES_HOME=/path/to/project`. `FINANCES_PORT` changes the port (default 8080).

The server only answers `Host: 127.0.0.1:<port>` / `localhost:<port>`, and non-GET requests must carry `HX-Request: true` (htmx adds it), which blocks cross-site forms and DNS rebinding. CMake generates `build/release/compile_commands.json` (or `build/debug/` with the `debug` preset).

### Tooling

- Warnings: GCC/Clang build with a strict set (`-Wconversion -Wsign-conversion -Wshadow -Wold-style-cast ...`); the code base is clean, keep it so.
- Sanitizers: `cmake --preset sanitize && cmake --build --preset sanitize && ctest --preset sanitize` (AddressSanitizer + UBSan; needs the debug Conan install).
- `.clang-format` / `.clang-tidy` are provided (`cmake --preset debug -DCMAKE_CXX_CLANG_TIDY=clang-tidy`).
- The build copies `templates/` and `static/` next to the binary, so `FINANCES_HOME=build/release ./build/release/finances` also works from anywhere (the database is then created in that folder).

## Usage

- **Dashboard**: current month balance, trend over the last 6 months and expenses by category.
- **This month / Other months**: list ordered by date (newest first), with its statistics and a form to add entries. The form updates the list and statistics without reloading the page (HTMX).
- Each entry has a title, amount, type (expense/income), category (Food, Transport, Housing, Leisure, Other) and date. The amount accepts a comma or a dot as the decimal separator, with at most two decimal places, and thousands separators (`12,50`, `1.234,56`, `1,234.56`, `1.234.567`). A lone `1.234` is rejected as ambiguous instead of being read as €1.23. Titles and names are limited in characters, not bytes.

## Tests

```sh
conan install . --output-folder=build --build=missing   # again: Catch2 is a new test dependency
ctest --preset release
```

- **Unit tests** (`tests/`, Catch2 v3): amounts, dates, validation, reports, the repository and `Ledger` on an in-memory database (shares, atomicity, debts, dashboard with a fixed clock) and the database migrations from a legacy file. Run them alone with `build/release/finances_tests`; skip building them with `-DFINANCES_BUILD_TESTS=OFF`.
- **Smoke test** (`test.sh`): starts the server on a random port with a temporary database and checks the dashboard, adding entries, validation, shares, the CSRF/Host guard and totals.
