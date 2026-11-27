#include "core/entry_form.hpp"
#include "core/calendar.hpp"
#include "core/errors.hpp"
#include "core/money.hpp"
#include "core/text.hpp"

namespace fin {

NewEntry validate(const EntryForm &form) {
  NewEntry entry;
  entry.amount = parse_amount(form.amount);

  const auto title = trim(form.title);
  const auto length = text_length(title);
  if (!length)
    throw ValidationError{"The title contains invalid characters."};
  if (*length == 0 || *length > kMaxTitleChars)
    throw ValidationError{"The title is required (maximum 100 characters)."};
  entry.title = std::string{title};

  const auto kind = kind_from_key(trim(form.kind));
  if (!kind)
    throw ValidationError{"Select expense or income."};
  entry.kind = *kind;

  const auto category = category_from_key(trim(form.category));
  if (!category)
    throw ValidationError{"Select a category."};
  entry.category = *category;

  const auto month = parse_year_month(trim(form.month));
  const auto date = parse_date(trim(form.date));
  if (!month || !date || month_of(*date) != *month)
    throw ValidationError{"The date must belong to the displayed month."};
  entry.date = *date;
  return entry;
}

} // namespace fin
