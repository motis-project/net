#include "net/ssl.h"

namespace net {

ssl::ssl(boost::asio::io_service& io_service, std::string host,
         std::string port, boost::posix_time::time_duration timeout)
    : ctx_(boost::asio::ssl::context::sslv23),
      resolver_(io_service),
      socket_(io_service, ctx_),
      req_timeout_timer_(io_service, std::move(timeout)),
      host_(std::move(host)),
      port_(std::move(port)),
      connected_(false) {
  boost::system::error_code ignore;
  ctx_.set_verify_mode(boost::asio::ssl::verify_none, ignore);
}

ssl::~ssl() {
  if (connected_) {
    cancel();
  }
}

void ssl::connect(connect_cb cb) {
  req_timeout_timer_.async_wait(std::bind(
      &ssl::timer_callback, this, shared_from_this(), std::placeholders::_1));
  return connect(shared_from_this(), std::move(cb));
}

void ssl::cancel() {
  connected_ = false;
  boost::system::error_code ignored;
  socket_.lowest_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_both,
                                  ignored);
  socket_.lowest_layer().close(ignored);
}

void ssl::connect(ssl_ptr self, connect_cb cb) {
  if (!connected_) {
    return resolve(std::move(self), std::move(cb));
  } else {
    return cb(shared_from_this(), boost::system::error_code());
  }
}

void ssl::resolve(ssl_ptr self, connect_cb cb) {
  boost::asio::ip::tcp::resolver::query query(host_, port_);
  return resolver_.async_resolve(
      query, std::bind(&ssl::on_resolve, this, std::move(self), std::move(cb),
                       std::placeholders::_1, std::placeholders::_2));
}

void ssl::on_resolve(ssl_ptr self, connect_cb cb, boost::system::error_code ec,
                     boost::asio::ip::tcp::resolver::iterator iterator) {
  if (!ec) {
    return boost::asio::async_connect(
        socket_.lowest_layer(), iterator,
        std::bind(&ssl::on_connect, this, std::move(self), std::move(cb),
                  std::placeholders::_1, std::placeholders::_2));
  } else {
    finally(ec);
    return cb(self, ec);
  }
}

void ssl::on_connect(ssl_ptr self, connect_cb cb, boost::system::error_code ec,
                     boost::asio::ip::tcp::resolver::iterator) {
  if (!ec) {
    return socket_.async_handshake(
        boost::asio::ssl::stream_base::client,
        std::bind(&ssl::on_handshake, this, self, cb, std::placeholders::_1));
  } else {
    finally(ec);
    return cb(self, ec);
  }
}

void ssl::on_handshake(ssl_ptr self, connect_cb cb,
                       boost::system::error_code ec) {
  if (!ec) {
    connected_ = true;
  } else {
    finally(ec);
  }
  return cb(self, ec);
}

void ssl::timer_callback(ssl_ptr, boost::system::error_code const& ec) {
  if (boost::asio::error::operation_aborted != ec) {
    cancel();
  }
}

void ssl::finally(boost::system::error_code const& ec) {
  req_timeout_timer_.cancel();
  if (ec == boost::asio::error::eof) {
    connected_ = false;
  }
}

}  // namespace net