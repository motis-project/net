#include "net/web_server/web_server.h"

#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/strand.hpp"
#include "net/web_server/detect_session.h"
#include "net/web_server/fail.h"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

namespace net {

struct web_server::impl {
  impl(asio::io_context& ioc, asio::ssl::context& ctx)
      : ioc_{ioc}, ctx_{ctx}, acceptor_{ioc} {}

  void on_http_request(http_req_cb_t cb) { http_req_cb_ = std::move(cb); }
  void on_ws_msg(ws_msg_cb_t cb) { ws_msg_cb_ = std::move(cb); }
  void on_ws_open(ws_open_cb_t cb) { ws_open_cb_ = std::move(cb); }
  void on_ws_close(ws_close_cb_t cb) { ws_close_cb_ = std::move(cb); }

  void init(std::string const& host, std::string const& port,
            boost::system::error_code& ec) {
    asio::ip::tcp::resolver resolver{ioc_};
    asio::ip::tcp::endpoint endpoint = *resolver.resolve({host, port});

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
      fail(ec, "open");
      return;
    }

    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
      fail(ec, "set_option");
      return;
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
      fail(ec, "bind");
      return;
    }

    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
      fail(ec, "listen");
      return;
    }
  }

  void run() {
    if (acceptor_.is_open()) {
      do_accept();
    }
  }

  void stop() { acceptor_.close(); }

  void do_accept() {
    acceptor_.async_accept(
        asio::make_strand(ioc_),
        boost::beast::bind_front_handler(&impl::on_accept, this));
  }

  void on_accept(boost::system::error_code ec, asio::ip::tcp::socket socket) {
    if (!acceptor_.is_open()) {
      return;
    }

    if (ec) {
      fail(ec, "main accept");
    } else {
      std::make_shared<detect_session>(std::move(socket), ctx_, http_req_cb_,
                                       ws_msg_cb_, ws_open_cb_, ws_close_cb_)
          ->run();
    }
    do_accept();
  }

  asio::io_context& ioc_;
  ssl::context& ctx_;
  tcp::acceptor acceptor_;

  http_req_cb_t http_req_cb_;
  ws_msg_cb_t ws_msg_cb_;
  ws_open_cb_t ws_open_cb_;
  ws_close_cb_t ws_close_cb_;
};

web_server::web_server(asio::io_context& ioc, asio::ssl::context& ctx)
    : impl_{std::make_unique<impl>(ioc, ctx)} {}

web_server::~web_server() = default;

void web_server::init(std::string const& host, std::string const& port,
                      boost::system::error_code& ec) {
  impl_->init(host, port, ec);
}

void web_server::run() { impl_->run(); }

void web_server::stop() { impl_->stop(); }

void web_server::on_http_request(http_req_cb_t cb) {
  impl_->on_http_request(std::move(cb));
}

void web_server::on_ws_msg(ws_msg_cb_t cb) { impl_->on_ws_msg(std::move(cb)); }

void web_server::on_ws_open(ws_open_cb_t cb) {
  impl_->on_ws_open(std::move(cb));
}

void web_server::on_ws_close(ws_close_cb_t cb) {
  impl_->on_ws_close(std::move(cb));
}

}  // namespace net