#pragma once

#include "core/types.hpp"
#include <string>
#include <string_view>

namespace fin {

/// Interprets an amount entered by the user, in cents. Throws `ValidationError`.
/// Accepted: "12", "12,5", "12.50", "1.234,56", "1,234.56", "1.234.567".
/// The last separator is the decimal one (max. 2 digits) when the amount has
/// two kinds of separator or a single one; separators before it group
/// thousands (1-3 digits, then groups of exactly 3). A lone "1.234" is
/// rejected as ambiguous; a repeated one ("1.234.567") is a thousands group.
[[nodiscard]] Cents parse_amount(std::string_view raw);

/// Spanish format: "1.234,56 €" (and "-5,00 €" for negative amounts).
[[nodiscard]] std::string format_money(Cents cents);

} // namespace fin
