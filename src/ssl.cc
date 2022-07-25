#include <utility>

#include "net/ssl.h"

namespace net {

ssl::ssl(boost::asio::io_service& io_service, std::string host,
         std::string port, boost::posix_time::time_duration const& timeout)
    : ctx_(boost::asio::ssl::context::sslv23),
      resolver_(io_service),
      socket_(io_service, ctx_),
      req_timeout_timer_(io_service, timeout),
      host_(std::move(host)),
      port_(std::move(port)),
      connected_(false) {
  boost::system::error_code ignore;
  ctx_.set_verify_mode(boost::asio::ssl::verify_none, ignore);
  ctx_.set_options(boost::asio::ssl::context::default_workarounds |
                   boost::asio::ssl::context::no_sslv2 |
                   boost::asio::ssl::context::no_sslv3 |
                   boost::asio::ssl::context::single_dh_use);
  // https://stackoverflow.com/a/59225060
  // SSL SNI extension
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-cstyle-cast)
  if (!SSL_set_tlsext_host_name(socket_.native_handle(), host_.c_str())) {
    throw boost::system::system_error{{static_cast<int>(::ERR_get_error()),
                                       boost::asio::error::get_ssl_category()}};
  }
}

ssl::~ssl() {
  if (connected_) {
    cancel();
  }
}

void ssl::connect(connect_cb cb) {
  req_timeout_timer_.async_wait(
      [me = shared_from_this()](boost::system::error_code const& ec) {
        me->timer_callback(me, ec);
      });
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
      query, [self = std::move(self), cb = std::move(cb)](
                 boost::system::error_code const& ec,
                 boost::asio::ip::tcp::resolver::iterator it) mutable {
        self->on_resolve(self, std::move(cb), ec, std::move(it));
      });
}

void ssl::on_resolve(ssl_ptr self, connect_cb cb, boost::system::error_code ec,
                     boost::asio::ip::tcp::resolver::iterator iterator) {
  if (!ec) {
    return boost::asio::async_connect(
        socket_.lowest_layer(), std::move(iterator),
        [self = std::move(self), cb = std::move(cb)](
            boost::system::error_code ec,
            boost::asio::ip::tcp::resolver::iterator const& it) {
          self->on_connect(self, cb, ec, it);
        });
  } else {
    finally(ec);
    return cb(self, ec);
  }
}

void ssl::on_connect(ssl_ptr const& self, const connect_cb& cb,
                     boost::system::error_code ec,
                     boost::asio::ip::tcp::resolver::iterator const&) {
  if (!ec) {
    return socket_.async_handshake(boost::asio::ssl::stream_base::client,
                                   [self, cb](boost::system::error_code ec) {
                                     self->on_handshake(self, cb, ec);
                                   });
  } else {
    finally(ec);
    return cb(self, ec);
  }
}

void ssl::on_handshake(ssl_ptr self, const connect_cb& cb,
                       boost::system::error_code ec) {
  if (!ec) {
    connected_ = true;
  } else {
    finally(ec);
  }
  return cb(std::move(self), ec);
}

void ssl::timer_callback(ssl_ptr const&, boost::system::error_code const& ec) {
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
