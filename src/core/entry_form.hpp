#pragma once

#include "core/types.hpp"
#include <cstddef>
#include <string_view>

namespace fin {

struct EntryForm {
  std::string_view title;
  std::string_view amount;
  std::string_view kind;
  std::string_view category;
  std::string_view date;
  std::string_view month;
};

inline constexpr std::size_t kMaxTitleChars = 100;
[[nodiscard]] NewEntry validate(const EntryForm &form);

} // namespace fin
