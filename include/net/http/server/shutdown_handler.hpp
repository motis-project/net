
#ifndef HTTP_SERVER_SHUTDOWN_HANDLER_HPP
#define HTTP_SERVER_SHUTDOWN_HANDLER_HPP

#include <boost/asio.hpp>

namespace net {
namespace http {
namespace server {

struct io_service_shutdown {
  io_service_shutdown(boost::asio::io_service& ios) : ios_(ios) {}
  void stop() { ios_.stop(); }
  boost::asio::io_service& ios_;
};

template <typename T>
class shutdown_handler {
public:
  shutdown_handler(boost::asio::io_service& ios, T& shutdown_subject)
      : shutdown_subject_(shutdown_subject), signals_(ios) {
    signals_.add(SIGINT);
    signals_.async_wait([this](boost::system::error_code /*ec*/,
                               int /*signo*/) { shutdown_subject_.stop(); });
  }

private:
  T& shutdown_subject_;
  boost::asio::signal_set signals_;
};

}  // namespace server
}  // namespace http
}  // namespace net

#endif  // HTTP_SERVER_SHUTDOWN_HANDLER_HPP
