#include "net/tcp.h"

namespace net {

tcp::tcp(boost::asio::io_service& io_service,
         std::string host, std::string port,
         boost::posix_time::time_duration timeout)
    : resolver_(io_service),
      socket_(io_service),
      req_timeout_timer_(io_service, std::move(timeout)),
      host_(std::move(host)),
      port_(std::move(port)),
      connected_(false) {
}

tcp::~tcp() {
  if (connected_) {
    cancel();
  }
}

void tcp::connect(connect_cb cb) {
  req_timeout_timer_.async_wait(std::bind(&tcp::timer_callback, this,
                                          shared_from_this(),
                                          std::placeholders::_1));
  return connect(shared_from_this(), std::move(cb));
}

void tcp::cancel() {
  connected_ = false;
  boost::system::error_code ignored;
  socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ignored);
  socket_.close(ignored);
}

void tcp::connect(tcp_ptr self, connect_cb cb) {
  if (!connected_) {
    return resolve(std::move(self), std::move(cb));
  } else {
    return cb(shared_from_this(), boost::system::error_code());
  }
}

void tcp::resolve(tcp_ptr self, connect_cb cb) {
  boost::asio::ip::tcp::resolver::query query(
      boost::asio::ip::tcp::v4(),
      host_, port_,
      boost::asio::ip::resolver_query_base::flags());
  return resolver_.async_resolve(query, std::bind(&tcp::on_resolve, this,
                                                  std::move(self),
                                                  std::move(cb),
                                                  std::placeholders::_1,
                                                  std::placeholders::_2));
}

void tcp::on_resolve(
    tcp_ptr self,
    connect_cb cb,
    boost::system::error_code ec,
    boost::asio::ip::tcp::resolver::iterator iterator) {
  if (!ec) {
    return boost::asio::async_connect(socket_, iterator,
                                      std::bind(&tcp::on_connect, this,
                                                std::move(self),
                                                std::move(cb),
                                                std::placeholders::_1,
                                                std::placeholders::_2));
  } else {
    finally(ec);
    return cb(self, ec);
  }
}

void tcp::on_connect(
    tcp_ptr self,
    connect_cb cb,
    boost::system::error_code ec,
    boost::asio::ip::tcp::resolver::iterator) {
  if (!ec) {
    connected_ = true;
  } else {
    finally(ec);
  }
  return cb(self, ec);
}

void tcp::timer_callback(tcp_ptr, boost::system::error_code const& ec) {
  if (boost::asio::error::operation_aborted != ec) {
    cancel();
  }
}

void tcp::finally(boost::system::error_code const& ec) {
  req_timeout_timer_.cancel();
  if (ec == boost::asio::error::eof) {
    connected_ = false;
  }
}

}  // namespace net