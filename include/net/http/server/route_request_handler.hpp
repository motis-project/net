#ifndef HTTP_ROUTE_REQUEST_HANDLER_HPP
#define HTTP_ROUTE_REQUEST_HANDLER_HPP

#include "net/http/server/request_handler.hpp"

namespace net {
namespace http {
namespace server {

class route_request;

typedef std::function<void(route_request const&, callback)> route_request_handler;

} // namespace server
} // namespace http
} // namespace net

#endif // HTTP_REQUEST_HANDLER_HPP
