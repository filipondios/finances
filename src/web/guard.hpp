#pragma once

#include <crow.h>

#include <cstdint>
#include <string>

namespace fin::web {

/// Defends the local server against requests from other web pages:
///  - `Host` must be the loopback address on our port (blocks DNS rebinding).
///  - Non-GET requests must carry `HX-Request: true`. htmx always sends it, and
///    a cross-site <form> cannot add custom headers without a CORS preflight,
///    which this server never answers (blocks CSRF).
struct LocalGuard {
  struct context {};

  std::uint16_t port = 8080;

  void before_handle(crow::request &req, crow::response &res, context &) const {
    const std::string suffix = ':' + std::to_string(port);
    const std::string host = req.get_header_value("Host");
    const bool host_ok = host == "127.0.0.1" + suffix || host == "localhost" + suffix;
    const bool mutating = req.method != crow::HTTPMethod::GET &&
                          req.method != crow::HTTPMethod::HEAD;
    if (!host_ok || (mutating && req.get_header_value("HX-Request") != "true")) {
      res.code = 403;
      res.body = "Forbidden";
      res.end();
    }
  }

  void after_handle(crow::request &, crow::response &res, context &) const {
    res.set_header("X-Frame-Options", "DENY");
    res.set_header("X-Content-Type-Options", "nosniff");
  }
};

using App = crow::App<LocalGuard>;

} // namespace fin::web
