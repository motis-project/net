#ifndef HTTP_QUERY_ROUTER_HPP
#define HTTP_QUERY_ROUTER_HPP

#include <regex>
#include <string>
#include <vector>


#include "net/http/server/route_request.hpp"
#include "net/http/server/reply.hpp"
#include "net/http/server/route_request_handler.hpp"

namespace net {
namespace http {
namespace server {

class query_router {
public:
  query_router& route(std::string method, std::string path_regex,
                      route_request_handler handler);
  void operator()(request const& req, callback cb);
  void reply_hook(std::function<void (reply&)> reply_hook);
  void enable_cors();

private:
  struct handler {
    std::string method;
    std::regex path;
    route_request_handler request_handler;
  };

  static void decode_content(request& req);

  static void set_credentials(route_request& req);

  static void set_content_length(reply& rep);

  static void set_content_type(reply& rep);

  static void set_status(reply& rep);

  static auto get_header(reply const& rep, std::string const& key)
    -> decltype(reply::headers)::const_iterator;

  std::vector<handler> routes_;
  std::function<void (reply&)> reply_hook_;
};

} // namespace server
} // namespace http
} // namespace net

#endif  // HTTP_QUERY_ROUTER_HPP