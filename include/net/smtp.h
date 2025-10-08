#ifndef NET_SMTP_H_
#define NET_SMTP_H_

#include <functional>
#include <istream>
#include <memory>
#include <string>

#include "boost/asio/coroutine.hpp"
#include "boost/asio/io_context.hpp"
#include "boost/asio/streambuf.hpp"
#include "boost/date_time/posix_time/posix_time_types.hpp"
#include "boost/system/error_code.hpp"

#include "net/ssl.h"

#define TIMEOUT (boost::posix_time::seconds(10))

namespace net {

struct smtp_request {
  std::string username;
  std::string password;
  std::string from;
  std::string to;
  std::string subject;
  std::string content;
};

// Very simple client for
// RFC5321 - Simple Mail Transfer Protocol
// RFC4954 - SMTP Service Extension for Authentication
// (using PLAIN authentication over SSL)
class smtp_client : public net::ssl, boost::asio::coroutine {
public:
  typedef std::function<void(std::shared_ptr<net::ssl>,
                             boost::system::error_code)>
      callback;

  smtp_client(boost::asio::io_context& ios, std::string host, std::string port,
              std::string hostname,
              boost::posix_time::time_duration timeout = TIMEOUT);

  void query(smtp_request& req, callback cb);

protected:
  void transfer(std::shared_ptr<net::ssl> self, callback cb,
                boost::system::error_code ec);

  void respond(callback cb, std::shared_ptr<net::ssl> self,
               boost::system::error_code ec);

  void generate_commands(smtp_request const& req);

  std::string hostname_;
  boost::asio::streambuf buf_;
  std::istream response_stream_;

  std::string init_cmd_;
  std::string auth_cmd_;
  std::string from_cmd_;
  std::string rcpt_cmd_;
  std::string data_cmd_;
  std::string data_;
  std::string quit_cmd_;
};

template <typename... Args>
std::shared_ptr<smtp_client> make_smtp_client(Args&&... args) {
  return std::make_shared<smtp_client>(std::forward<Args>(args)...);
}

}  // namespace net

#endif  // NET_SMTP_H_