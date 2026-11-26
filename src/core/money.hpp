#pragma once

#include "core/types.hpp"
#include <string>
#include <string_view>

namespace fin {

/// Interprets an amount entered by the user
[[nodiscard]] Cents parse_amount(std::string_view raw);

/// Spanish format: "1.234,56 €" (and "-5,00 €" for negative amounts).
[[nodiscard]] std::string format_money(Cents cents);

} // namespace fin
