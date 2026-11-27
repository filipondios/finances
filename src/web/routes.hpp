#pragma once

#include "app/ledger.hpp"
#include "web/guard.hpp"
#include "web/views.hpp"

#include <crow.h>

namespace fin::web {

/// Registers the HTTP routes. `ledger` and `views` must outlive `app`.
void register_routes(App &app, Ledger &ledger, const Views &views);

} // namespace fin::web
