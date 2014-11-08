#ifndef NET_HTTP_CLIENT_HTTP_CLIENT_H_
#define NET_HTTP_CLIENT_HTTP_CLIENT_H_

#include "net/http/client/client.h"
#include "net/ssl.h"

namespace net {
namespace http {
namespace client {

typedef basic_http_client<ssl> https;

template<typename... Args>
std::shared_ptr<https> make_https(Args&&... args) {
  return std::make_shared<https>(std::forward<Args>(args) ...);
}

}  // namespace client
}  // namespace http
}  // namespace net

#endif  // NET_HTTP_CLIENT_HTTP_CLIENT_H_