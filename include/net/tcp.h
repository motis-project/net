#ifndef NET_TCP_H_
#define NET_TCP_H_

#include <functional>
#include <memory>
#include <string>

#include "boost/asio.hpp"
#include "boost/date_time/posix_time/posix_time_types.hpp"

namespace net {

class tcp : public std::enable_shared_from_this<tcp> {
public:
  typedef std::shared_ptr<tcp> tcp_ptr;
  typedef std::function<void(tcp_ptr, boost::system::error_code)> connect_cb;

  tcp(boost::asio::io_service&, std::string host, std::string port,
      boost::posix_time::time_duration timeout);

  tcp(boost::asio::io_service&, std::string host, std::string port);

  virtual ~tcp();

  void connect(connect_cb cb);

  virtual void cancel();

  void connect(tcp_ptr self, connect_cb cb);

  void resolve(tcp_ptr self, connect_cb cb);

  void on_resolve(tcp_ptr self, connect_cb cb, boost::system::error_code ec,
                  boost::asio::ip::tcp::resolver::iterator iterator);

  void on_connect(tcp_ptr self, connect_cb cb, boost::system::error_code ec,
                  boost::asio::ip::tcp::resolver::iterator);

  void timer_callback(tcp_ptr self, boost::system::error_code const& ec);

  void finally(boost::system::error_code const& ec);

  boost::asio::ip::tcp::resolver resolver_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::deadline_timer req_timeout_timer_;
  std::string host_, port_;
  bool use_timeout_, connected_;
};

}  // namespace net

#endif  // NET_TCP_H_