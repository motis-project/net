#include "net/web_server/web_server.h"

#include <iostream>
#include <set>

#include "boost/asio/ip/tcp.hpp"
#include "net/web_server/http_session.h"
#include "net/web_server/ssl_stream.h"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

namespace net {

inline void fail(boost::system::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

struct web_server::impl {
  impl(asio::io_context& ioc, asio::ssl::context& ctx)
      : ctx_{ctx}, acceptor_{ioc}, socket_{ioc} {}

  void on_http_request(http_req_cb_t cb) { http_req_cb_ = std::move(cb); }
  void on_ws_msg(ws_msg_cb_t cb) { ws_msg_cb_ = std::move(cb); }
  void on_ws_open(ws_open_cb_t cb) { ws_open_cb_ = std::move(cb); }
  void on_ws_close(ws_close_cb_t cb) { ws_close_cb_ = std::move(cb); }

  void init(std::string const& host, std::string const& port,
            boost::system::error_code& ec) {
    asio::ip::tcp::resolver resolver{socket_.get_executor()};
    asio::ip::tcp::endpoint endpoint = *resolver.resolve({host, port});

    acceptor_.open(endpoint.protocol(), ec);
    if (ec) {
      return;
    }

    acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) {
      return;
    }

    acceptor_.bind(endpoint, ec);
    if (ec) {
      return;
    }

    acceptor_.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) {
      return;
    }
  }

  void run() {
    if (acceptor_.is_open()) {
      do_accept();
    }
  }

  void stop() {
    acceptor_.close();
    for (auto const& s : session_mgr_.sessions_) {
      s->stop();
    }
  }

  void do_accept() {
    acceptor_.async_accept(
        socket_, std::bind(&impl::on_accept, this, std::placeholders::_1));
  }

  void on_accept(boost::system::error_code ec) {
    if (!acceptor_.is_open()) {
      return;
    }

    if (ec) {
      fail(ec, "main accept");
    } else {
      std::make_shared<http_session>(session_mgr_, std::move(socket_), ctx_,
                                     http_req_cb_, ws_msg_cb_, ws_open_cb_,
                                     ws_close_cb_)
          ->run();
    }
    do_accept();
  }

  ssl::context& ctx_;
  tcp::acceptor acceptor_;
  tcp::socket socket_;
  session_manager session_mgr_;

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

session_manager const* web_server::get_session_mgr() const {
  return &impl_->session_mgr_;
}

}  // namespace net