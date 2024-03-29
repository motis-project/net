#ifndef NET_HTTP_CLIENT_REQUEST_H_
#define NET_HTTP_CLIENT_REQUEST_H_

#include <map>
#include <optional>
#include <string>

#include "net/http/client/url.h"

namespace net {
namespace http {
namespace client {

class request {
public:
  typedef std::map<std::string, std::string> str_map;

  enum method { GET, POST, DEL, PUT, OPTIONS, CONNECT };

  request(url addr, enum method m = GET, str_map hdr = str_map(),
          std::string body = "");
  request(std::string const&);
  request(char const* s);

  bool use_https() const;
  bool use_http() const;
  url peer() const;
  request set_proxy(url const&);
  std::string to_str() const;

  std::optional<url> proxy;
  url address;
  method req_method;
  std::map<std::string, std::string> headers;
  std::string body;
};

char const* method_to_str(request::method);

}  // namespace client
}  // namespace http
}  // namespace net

#endif  // NET_HTTP_CLIENT_REQUEST_H_
