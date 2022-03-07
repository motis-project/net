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
  url(char const* address);

  std::string const& str() const { return str_; }
  std::string const& host() const { return host_; }
  std::string const& port() const { return port_; }
  std::string const& path() const { return path_; }
  std::string const& prot() const { return prot_; }

private:
  std::string str_;
  std::string prot_;
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
