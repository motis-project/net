#ifndef HTTP_SERVER_ENABLE_CORS_HPP
#define HTTP_SERVER_ENABLE_CORS_HPP

#include "net/http/server/reply.hpp"

namespace net {
namespace http {
namespace server {

struct enable_cors {
  enable_cors(http::server::reply& rep) : rep_(rep) {}
  ~enable_cors() {
    rep_.headers.emplace_back("Access-Control-Allow-Origin", "*");
    rep_.headers.emplace_back(
        "Access-Control-Allow-Headers",
        "X-Requested-With, Content-Type, Accept, Authorization");
    rep_.headers.emplace_back("Access-Control-Allow-Methods",
                              "GET, POST, PUT, DELETE, OPTIONS, HEAD");
  }
  http::server::reply& rep_;
};

inline void add_cors_headers(http::server::reply& rep) {
  enable_cors cors(rep);
}

}  // namespace server
}  // namespace http
}  // namespace net

#endif  // HTTP_SERVER_ENABLE_CORS_HPP
