#pragma once

#include "core/types.hpp"
#include <SQLiteCpp/SQLiteCpp.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <utility>

namespace fin::test {

inline Date ymd(int y, unsigned m, unsigned d) {
  return Date{std::chrono::year{y}, std::chrono::month{m}, std::chrono::day{d}};
}

inline YearMonth ym(int y, unsigned m) {
  return YearMonth{std::chrono::year{y}, std::chrono::month{m}};
}

inline NewEntry make_entry(std::string title, Cents cents, Kind kind,
                           Category category, Date date) {
  return {.title = std::move(title),
          .amount = cents,
          .kind = kind,
          .category = category,
          .date = date};
}

/// A database file in the temp directory, removed (with its WAL files) on exit.
class TempDb {
public:
  TempDb() {
    static std::atomic<unsigned> counter{0};
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    path_ = std::filesystem::temp_directory_path() /
            ("finances-test-" + std::to_string(stamp) + "-" +
             std::to_string(counter++) + ".db");
  }
  ~TempDb() {
    std::error_code ec;
    for (const char *suffix : {"", "-wal", "-shm"})
      std::filesystem::remove(path_.string() + suffix, ec);
  }
  TempDb(const TempDb &) = delete;
  TempDb &operator=(const TempDb &) = delete;

  [[nodiscard]] const std::filesystem::path &path() const { return path_; }

  /// Direct SQL access, bypassing the repository.
  [[nodiscard]] SQLite::Database raw() const {
    return SQLite::Database{path_.string(),
                            SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE};
  }

private:
  std::filesystem::path path_;
};

} // namespace fin::test
