#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace fin {

using Cents = std::int64_t;
using Date = std::chrono::year_month_day;
using YearMonth = std::chrono::year_month;
enum class Kind : std::uint8_t { expense, income };

[[nodiscard]] constexpr std::string_view key(Kind k) noexcept {
  return k == Kind::income ? "income" : "expense";
}

[[nodiscard]] constexpr std::string_view label(Kind k) noexcept {
  return k == Kind::income ? "Income" : "Expense";
}

[[nodiscard]] constexpr std::optional<Kind>
kind_from_key(std::string_view s) noexcept {
  if (s == "expense")
    return Kind::expense;
  if (s == "income")
    return Kind::income;
  return std::nullopt;
}

/// To add a category: append it here (keeping `other` last) and in
/// `kCategories`. Nothing else needs to change: the form options are built
/// from `kCategories`, and the database does not constrain the value.
enum class Category : std::uint8_t {
  food,
  transport,
  housing,
  leisure,
  other
};

inline constexpr std::size_t kCategoryCount =
    static_cast<std::size_t>(Category::other) + 1;

struct CategoryInfo {
  std::string_view key;
  std::string_view label;
};

/// Indexed by `Category`; the order must match the enum.
inline constexpr std::array<CategoryInfo, kCategoryCount> kCategories{{
    {"alimentacion", "Food"},
    {"transporte", "Transport"},
    {"vivienda", "Housing"},
    {"ocio", "Leisure"},
    {"otros", "Other"},
}};

[[nodiscard]] constexpr const CategoryInfo &info(Category c) noexcept {
  return kCategories[static_cast<std::size_t>(c)];
}

[[nodiscard]] constexpr std::optional<Category>
category_from_key(std::string_view s) noexcept {
  for (std::size_t i = 0; i < kCategories.size(); ++i)
    if (kCategories[i].key == s)
      return static_cast<Category>(i);
  return std::nullopt;
}

/// Every enumerator has a distinct, non-empty key and label, and keys map back
/// to their category (a missing or misordered entry fails to compile).
[[nodiscard]] constexpr bool categories_consistent() noexcept {
  for (std::size_t i = 0; i < kCategoryCount; ++i) {
    if (kCategories[i].key.empty() || kCategories[i].label.empty())
      return false;
    if (category_from_key(kCategories[i].key) != static_cast<Category>(i))
      return false;
  }
  return true;
}
static_assert(categories_consistent(),
              "kCategories must list every Category, in enum order");

/// Amount accumulated per category, indexed by `Category`.
using CategoryTotals = std::array<Cents, kCategoryCount>;

/// An already validated entry
struct NewEntry {
  std::string title;
  Cents amount{};
  Kind kind{};
  Category category{};
  Date date{};
};

/// An entry read from the DB
struct Entry {
  std::int64_t id{};
  std::string title;
  Cents amount{};
  Kind kind{};
  Category category{};
  Date date{};
};

struct Person {
  std::int64_t id{};
  std::string name;
};

struct Share {
  std::int64_t person_id{};
  std::string name;
  Cents amount{};
};

struct PersonDebt {
  std::int64_t id{};
  std::string name;
  Cents total{};
  Cents month_total{};
};

struct MonthTotals {
  Cents income{};
  Cents expenses{};
  int count{};

  [[nodiscard]] constexpr Cents balance() const noexcept {
    return income - expenses;
  }
};

} // namespace fin
