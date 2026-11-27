# Finances

Local app to record expenses and income and view them by month. C++20 with Crow, SQLite (via SQLiteCpp) and HTML + HTMX/Chart.js.

## Getting started

You need CMake (≥ 3.23, for the presets), Conan 2 and a compiler with C++20 (`std::format` and the `std::chrono` calendar: GCC ≥ 13, Clang ≥ 17 with libstdc++ 13, MSVC 2022). Crow, Asio and SQLiteCpp (with SQLite3) are managed with Conan; HTMX and Chart.js are served as local static files.

```sh
conan profile detect          # first time only
conan install . --output-folder=build --build=missing
conan install . --output-folder=build --build=missing -s build_type=Debug   # only if you use the debug preset
cmake --preset release
cmake --build --preset release
./build/release/finances
```

Open <http://127.0.0.1:8080>. The executable reads `templates/` and `static/` and creates `finances.db` in the current working directory, so run it from here (it refuses to start if the templates are missing) or set `FINANCES_HOME=/path/to/project`. `FINANCES_PORT` changes the port (default 8080).

The server only answers `Host: 127.0.0.1:<port>` / `localhost:<port>`, and non-GET requests must carry `HX-Request: true` (htmx adds it), which blocks cross-site forms and DNS rebinding. CMake generates `build/release/compile_commands.json` (or `build/debug/` with the `debug` preset).

## Usage

- **Dashboard**: current month balance, trend over the last 6 months and expenses by category.
- **This month / Other months**: list ordered by insertion date (newest first), with its statistics and a form to add entries. The form updates the list and statistics without reloading the page (HTMX).
- Each entry has a title, amount, type (expense/income), category (Food, Transport, Housing, Leisure, Other) and date. The amount accepts a comma or a dot as the decimal separator, with at most two decimal places (`12,50`; `1.234` is rejected instead of being read as €1.23).

## Tests

```sh
ctest --preset release
```

Starts the server with a temporary database and checks the dashboard, adding entries, validation and totals.
