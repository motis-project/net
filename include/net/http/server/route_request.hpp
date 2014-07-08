#ifndef HTTP_SERVER_ROUTE_REQUEST_HPP
#define HTTP_SERVER_ROUTE_REQUEST_HPP

#include <string>
#include <vector>

#include "net/http/server/request.hpp"

namespace net {
namespace http {
namespace server {

class route_request : public request {
public:
  route_request(request req)
    : request(req) {
  }

  std::vector<std::string> path_params;
  std::string username, password;
};

} // namespace server
} // namespace http
} // namespace net

#endif  // HTTP_SERVER_ROUTE_REQUEST_HPP