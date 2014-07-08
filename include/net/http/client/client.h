#ifndef NET_HTTP_CLIENT_CLIENT_H_
#define NET_HTTP_CLIENT_CLIENT_H_

#include <functional>
#include <memory>
#include <string>

#include "boost/asio.hpp"
#include "boost/asio/coroutine.hpp"
#include "boost/iostreams/stream.hpp"
#include "boost/system/error_code.hpp"

#include "net/tcp.h"
#include "net/ssl.h"
#include "net/http/client/url.h"
#include "net/http/client/request.h"
#include "net/http/client/response.h"

#define TIMEOUT (boost::posix_time::seconds(10))

namespace net {
namespace http {
namespace client {

template<typename C = tcp>
class basic_http_client : public C, boost::asio::coroutine {
public:
  typedef char char_type;
  typedef boost::iostreams::source_tag category;

  typedef std::function<void(std::shared_ptr<C>,
                             response,
                             boost::system::error_code)> callback;

  basic_http_client(boost::asio::io_service& ios, url u,
                    boost::posix_time::time_duration timeout = TIMEOUT);

  void query(request& req, callback cb);

  std::streamsize read(char_type* s, std::streamsize n);

protected:
  void transfer(std::shared_ptr<C> self,
                callback cb,
                boost::system::error_code ec);

  void respond(callback cb,
               std::shared_ptr<C> self,
               boost::system::error_code ec);

  void read_header();

  void read_content_length();

  std::size_t copy_content(std::size_t buffer_size);

  std::string request_;
  boost::asio::streambuf buf_;
  std::istream response_stream_;
  int status_code_;
  std::size_t length_;
  std::map<std::string, std::string> header_;
  std::vector<char> response_;
};

typedef basic_http_client<tcp> http;
typedef basic_http_client<ssl> https;

template<typename... Args>
std::shared_ptr<http> make_http(Args&&... args) {
  return std::make_shared<http>(std::forward<Args>(args) ...);
}

template<typename... Args>
std::shared_ptr<https> make_https(Args&&... args) {
  return std::make_shared<https>(std::forward<Args>(args) ...);
}

}  // namespace client
}  // namespace http
}  // namespace net

#endif  // NET_HTTP_CLIENT_CLIENT_H_