#ifndef NET_HTTP_CLIENT_URL_H_
#define NET_HTTP_CLIENT_URL_H_

#include <string>

namespace net {
namespace http {
namespace client {

class url {
public:
  url() = default;
  url(std::string host, std::string port, std::string path);
  url(const std::string& url);
  url(char const* url);

  std::string str() const { return str_; }
  std::string host() const { return host_; }
  std::string port() const { return port_; }
  std::string path() const { return path_; }

private:
  std::string str_;
  std::string host_;
  std::string port_;
  std::string path_;
};

inline std::ostream& operator<<(std::ostream& out, url const& u) {
  return out << u.str();
}

}  // namespace client
}  // namespace http
}  // namespace net

#endif  // NET_HTTP_CLIENT_URL_H_
