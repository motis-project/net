#ifndef NET_HTTP_CLIENT_RESPONSE_H_
#define NET_HTTP_CLIENT_RESPONSE_H_

#include <string>
#include <map>

namespace net {
namespace http {
namespace client {

struct response {
  std::map<std::string, std::string> headers;
  std::string body;
};

}  // namespace client
}  // namespace http
}  // namespace net

#endif  // NET_HTTP_CLIENT_RESPONSE_H_