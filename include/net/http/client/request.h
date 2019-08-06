#ifndef NET_HTTP_CLIENT_REQUEST_H_
#define NET_HTTP_CLIENT_REQUEST_H_

#include <map>
#include <string>

#include "net/http/client/url.h"

namespace net {
namespace http {
namespace client {

class request {
public:
  typedef std::map<std::string, std::string> str_map;

  enum method { GET, POST, DEL, PUT, OPTIONS };

  request(std::string const& addr, enum method m = GET, str_map hdr = str_map(),
          std::string body = "");

  std::string to_str() const;

  url address;
  method req_method;
  std::map<std::string, std::string> headers;
  std::string body;
};

}  // namespace client
}  // namespace http
}  // namespace net

#endif  // NET_HTTP_CLIENT_REQUEST_H_