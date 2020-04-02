#pragma once

#include "net/web_server/web_server.h"

namespace net {

template <typename Response>
void enable_cors(Response& res) {
  using namespace boost::beast::http;
  std::visit(
      [](auto& v) {
        v.base().set(field::access_control_allow_origin, "*");
        v.base().set(field::access_control_allow_headers,
                     "X-Requested-With, Content-Type, Accept, Authorization");
        v.base().set(field::access_control_allow_methods, "GET, POST, OPTIONS");
      },
      res);
}

}  // namespace net