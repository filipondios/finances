#pragma once

#include <stdexcept>

namespace fin {

class ValidationError : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

} // namespace fin
