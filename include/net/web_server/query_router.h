#include <regex>
#include <string>
#include <vector>

#include "net/web_server/web_server.h"

namespace net {

class query_router {
public:
  using request = web_server::http_req_t;
  using reply = web_server::http_res_t;

  struct route_request : public web_server::http_req_t {
  public:
    route_request(request req) : web_server::http_req_t(req) {}
    std::vector<std::string> path_params;
    std::string username, password;
  };

  using route_request_handler = std::function<void(
      route_request const&, web_server::http_res_cb_t, bool)>;

  query_router& route(std::string method, std::string path_regex,
                      route_request_handler handler);
  void operator()(web_server::http_req_t, web_server::http_res_cb_t, bool);
  void reply_hook(std::function<void(web_server::http_res_t&)> reply_hook);
  void enable_cors();

private:
  struct handler {
    std::string method;
    std::regex path;
    route_request_handler request_handler;
  };

  static void decode_content(request& req);

  static void set_credentials(route_request& req);

  std::vector<handler> routes_;
  std::function<void(reply&)> reply_hook_;
};

}  // namespace net