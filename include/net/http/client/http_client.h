#ifndef NET_HTTP_CLIENT_HTTP_CLIENT_H_
#define NET_HTTP_CLIENT_HTTP_CLIENT_H_

#include "net/http/client/client.h"
#include "net/tcp.h"

namespace net {
namespace http {
namespace client {

typedef basic_http_client<tcp> http;

template <typename... Args>
std::shared_ptr<http> make_http(Args&&... args) {
  return std::make_shared<http>(std::forward<Args>(args)...);
}

}  // namespace client
}  // namespace http
}  // namespace net

#endif  // NET_HTTP_CLIENT_HTTP_CLIENT_H_