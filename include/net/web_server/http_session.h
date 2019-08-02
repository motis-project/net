#pragma once

#include <iostream>
#include <memory>

#include "boost/asio/coroutine.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/post.hpp"
#include "boost/asio/ssl/context.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/ssl/ssl_stream.hpp"
#include "boost/beast/version.hpp"
#include "boost/beast/websocket/rfc6455.hpp"

#include "net/web_server/session_manager.h"
#include "net/web_server/web_server.h"

namespace net {

inline void fail(boost::system::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

template <class Derived>
struct http_session : public session /*, public boost::asio::coroutine*/ {
  http_session(session_manager& session_mgr, boost::beast::flat_buffer&& buffer,
               web_server::http_req_cb_t& http_req_cb,
               web_server::ws_msg_cb_t& ws_msg_cb,
               web_server::ws_open_cb_t& ws_open_cb,
               web_server::ws_close_cb_t& ws_close_cb)
      : session_mgr_{session_mgr},
        buffer_{std::move(buffer)},
        http_req_cb_{http_req_cb},
        ws_msg_cb_{ws_msg_cb},
        ws_open_cb_{ws_open_cb},
        ws_close_cb_{ws_close_cb} {
    session_mgr_.add(this);
  }

  ~http_session() override;

  http_session(http_session const&) = delete;
  http_session(http_session&&) = delete;

  http_session& operator=(http_session const&) = delete;
  http_session& operator=(http_session&&) = delete;

  // void run();
  // void stop() override;

  Derived& derived() { return static_cast<Derived&>(*this); }

protected:
  void do_read() {
    boost::beast::get_lowest_layer(derived().stream())
        .expires_after(std::chrono::seconds(30));
    boost::beast::http::async_read(
        derived().stream(), buffer_, req_,
        boost::beast::bind_front_handler(&http_session::on_read,
                                         derived().shared_from_this()));
  }

  void on_read(boost::beast::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec == boost::beast::http::error::end_of_stream) {
      return derived().do_eof();
    } else if (ec == boost::asio::error::operation_aborted) {
      return;
    } else if (ec) {
      return fail(ec, "read");
    } else if (boost::beast::websocket::is_upgrade(req_)) {
      boost::beast::get_lowest_layer(derived().stream()).expires_never();
      /*
      auto websocket = std::make_shared<ws_session>(
          session_mgr_, derived().release_stream(), ws_msg_cb_, ws_close_cb_);
      if (ws_open_cb_) {
        asio::post(
            websocket->ws_.get_executor(),
            [cb = &ws_open_cb_, websocket, this]() { (*cb)(websocket); });
      }
      websocket->run(std::move(req_));
       */
      return;
    }

    if (http_req_cb_) {
      http_req_cb_(std::move(req_), [this, self = derived().shared_from_this()](
                                        web_server::http_res_t res) {
        res_ = std::move(res);
        boost::asio::post(
            boost::beast::get_lowest_layer(derived().stream()).get_executor(),
            std::bind(&http_session::do_write, self));
      });
    } else {
      res_ = not_found(req_);
      do_write();
    }
  }

  void do_write() {
    boost::beast::http::async_write(
        derived().stream(), res_,
        boost::beast::bind_front_handler(&http_session::on_write,
                                         derived().shared_from_this(),
                                         res_.need_eof()));
  }

  void on_write(bool close, boost::beast::error_code ec,
                std::size_t /*bytes_transferred*/) {
    if (ec) {
      return fail(ec, "http write");
    }
    if (close) {
      return derived().do_eof();
    }
    res_ = {};
    do_read();
  };

  session_manager& session_mgr_;

  boost::beast::flat_buffer buffer_;

  web_server::http_req_t req_;
  web_server::http_res_t res_;

  web_server::http_req_cb_t& http_req_cb_;
  web_server::ws_msg_cb_t& ws_msg_cb_;
  web_server::ws_open_cb_t& ws_open_cb_;
  web_server::ws_close_cb_t& ws_close_cb_;
};

struct plain_http_session
    : public http_session<plain_http_session>,
      public std::enable_shared_from_this<plain_http_session> {

  plain_http_session(session_manager& session_mgr,
                     boost::beast::tcp_stream&& stream,
                     boost::beast::flat_buffer&& buffer,
                     web_server::http_req_cb_t& http_req_cb,
                     web_server::ws_msg_cb_t& ws_msg_cb,
                     web_server::ws_open_cb_t& ws_open_cb,
                     web_server::ws_close_cb_t& ws_close_cb)
      : http_session<plain_http_session>{session_mgr, std::move(buffer),
                                         http_req_cb, ws_msg_cb,
                                         ws_open_cb,  ws_close_cb},
        stream_{std::move(stream)} {}

  boost::beast::tcp_stream& stream() { return stream_; }

  boost::beast::tcp_stream release_stream() { return std::move(stream_); }

  void run() { do_read(); }

  void do_eof() {
    boost::beast::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
  }

private:
  boost::beast::tcp_stream stream_;
};

struct ssl_http_session
    : public http_session<ssl_http_session>,
      public std::enable_shared_from_this<ssl_http_session> {
  ssl_http_session(session_manager& session_mgr,
                   boost::beast::tcp_stream&& stream,
                   boost::beast::flat_buffer&& buffer,
                   boost::asio::ssl::context& ctx,
                   web_server::http_req_cb_t& http_req_cb,
                   web_server::ws_msg_cb_t& ws_msg_cb,
                   web_server::ws_open_cb_t& ws_open_cb,
                   web_server::ws_close_cb_t& ws_close_cb)
      : http_session<ssl_http_session>{session_mgr, std::move(buffer),
                                       http_req_cb, ws_msg_cb,
                                       ws_open_cb,  ws_close_cb},
        stream_{std::move(stream), ctx} {}

  boost::beast::ssl_stream<boost::beast::tcp_stream>& stream() {
    return stream_;
  }

  boost::beast::ssl_stream<boost::beast::tcp_stream> release_stream() {
    return std::move(stream_);
  }

  void do_eof() {
    boost::beast::get_lowest_layer(stream_).expires_after(
        std::chrono::seconds(30));
    // stream_.async_shutdown()
  }

private:
  boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
};

}  // namespace net