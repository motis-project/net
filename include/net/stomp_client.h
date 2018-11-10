#ifndef NET_STOMP_CLIENT_H_
#define NET_STOMP_CLIENT_H_

#include <functional>
#include <memory>
#include <string>

#include "boost/asio/coroutine.hpp"
#include "boost/asio/deadline_timer.hpp"
#include "boost/asio/streambuf.hpp"
#include "boost/system/error_code.hpp"

#include "net/tcp.h"

namespace net {

class stomp_client : public net::tcp, boost::asio::coroutine {
public:
  typedef std::function<void(std::shared_ptr<net::tcp>,
                             boost::system::error_code)>
      callback;

  stomp_client(boost::asio::io_service& ios, std::string host, std::string port,
               std::string destination);

  void subscribe(callback bail_out, std::function<void(std::string)> on_msg);
  virtual void cancel() override;

private:
  void init_commands();

  void transfer(net::tcp::tcp_ptr self, callback cb,
                std::function<void(std::string)> on_msg,
                boost::system::error_code ec);

  void skip_whitespace();

  void push_server_timeout();

  void server_timeout(net::tcp::tcp_ptr self, boost::system::error_code ec);

  void heartbeat(net::tcp::tcp_ptr self, boost::system::error_code ec);

  void respond(callback cb, net::tcp::tcp_ptr self,
               boost::system::error_code ec);

  std::string destination_;
  boost::asio::streambuf buf_;
  boost::asio::deadline_timer beat_timer_;
  boost::asio::deadline_timer beat_timeout_timer_;

  std::string connect_cmd_;
  std::string subscribe_cmd_;
  std::string beat_cmd_;
};

}  // namespace net

#endif  //  NET_STOMP_CLIENT_H_