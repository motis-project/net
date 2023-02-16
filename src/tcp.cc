#include "net/tcp.h"

namespace asio = boost::asio;

namespace net {

tcp::tcp(asio::io_service& ios, std::string host, std::string port,
         boost::posix_time::time_duration const& timeout)
    : resolver_(ios),
      socket_(ios),
      req_timeout_timer_(ios, timeout),
      host_(std::move(host)),
      port_(std::move(port)),
      use_timeout_(true),
      connected_(false) {}

tcp::tcp(asio::io_service& ios, std::string host, std::string port)
    : resolver_(ios),
      socket_(ios),
      req_timeout_timer_(ios),
      host_(std::move(host)),
      port_(std::move(port)),
      use_timeout_(false),
      connected_(false) {}

tcp::~tcp() {
  if (connected_) {
    cancel();
  }
}

void tcp::connect(connect_cb cb) {
  if (use_timeout_) {
    req_timeout_timer_.async_wait(
        [me = shared_from_this()](boost::system::error_code const& ec) {
          me->timer_callback(me, ec);
        });
  }
  return connect(shared_from_this(), std::move(cb));
}

void tcp::cancel() {
  connected_ = false;
  boost::system::error_code ignored;
  socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
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
  asio::ip::tcp::resolver::query const query(
      asio::ip::tcp::v4(), host_, port_,
      asio::ip::resolver_query_base::flags());
  return resolver_.async_resolve(
      query, [self = std::move(self), cb = std::move(cb)](
                 boost::system::error_code ec,
                 asio::ip::tcp::resolver::iterator iterator) mutable {
        self->on_resolve(self, std::move(cb), ec, std::move(iterator));
      });
}

void tcp::on_resolve(tcp_ptr self, connect_cb cb, boost::system::error_code ec,
                     asio::ip::tcp::resolver::iterator iterator) {
  if (!ec) {
    return asio::async_connect(
        socket_, std::move(iterator),
        [self = std::move(self), cb = std::move(cb)](
            boost::system::error_code const& ec,
            asio::ip::tcp::resolver::iterator const& it) {
          self->on_connect(self, cb, ec, it);
        });
  } else {
    finally(ec);
    return cb(self, ec);
  }
}

void tcp::on_connect(tcp_ptr self, const connect_cb& cb,
                     boost::system::error_code ec,
                     asio::ip::tcp::resolver::iterator const&) {
  if (!ec) {
    connected_ = true;
  } else {
    finally(ec);
  }
  return cb(std::move(self), ec);
}

void tcp::timer_callback(tcp_ptr const&, boost::system::error_code const& ec) {
  if (asio::error::operation_aborted != ec) {
    cancel();
  }
}

void tcp::finally(boost::system::error_code const& ec) {
  req_timeout_timer_.cancel();
  if (ec == asio::error::eof) {
    connected_ = false;
  }
}

}  // namespace net