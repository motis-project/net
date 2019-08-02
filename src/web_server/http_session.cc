#include "net/web_server/http_session.h"

#include <iostream>

#include "boost/asio/post.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/version.hpp"
#include "boost/beast/websocket/rfc6455.hpp"

#include "net/web_server/ws_session.h"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
namespace http = boost::beast::http;
using tcp = asio::ip::tcp;

namespace net {

inline void fail(boost::system::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

web_server::http_res_t not_found(web_server::http_req_t const& req) {
  web_server::http_res_t res{http::status::not_found, req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, "text/html");
  res.keep_alive(req.keep_alive());
  res.body() = "No handler implemented.";
  res.prepare_payload();
  return res;
}

http_session::http_session(session_manager& session_mgr, tcp::socket socket,
                           boost::beast::flat_buffer&& buffer,
                           ssl::context& ctx,
                           web_server::http_req_cb_t& http_req_cb,
                           web_server::ws_msg_cb_t& ws_msg_cb,
                           web_server::ws_open_cb_t& ws_open_cb,
                           web_server::ws_close_cb_t& ws_close_cb)
    : session_mgr_{session_mgr},
      stream_{std::move(socket), ctx},
      buffer_{std::move(buffer)},
      http_req_cb_{http_req_cb},
      ws_msg_cb_{ws_msg_cb},
      ws_open_cb_{ws_open_cb},
      ws_close_cb_{ws_close_cb} {
  session_mgr_.add(this);
}

http_session::~http_session() { session_mgr_.remove(this); }

void http_session::run() { loop({}, 0, false); }

void http_session::stop() { boost::beast::get_lowest_layer(stream_).close(); }

#include "boost/asio/yield.hpp"
void http_session::loop(boost::system::error_code ec, std::size_t bytes_used,
                        bool close) {
  reenter(*this) {
    yield stream_.async_handshake(
        ssl::stream_base::server, buffer_.data(),
        std::bind(&http_session::loop, shared_from_this(),
                  std::placeholders::_1, std::placeholders::_2, false));
    if (ec) {
      return fail(ec, "http handshake");
    }

    buffer_.consume(bytes_used);

    while (true) {
      req_ = {};

      yield http::async_read(
          stream_, buffer_, req_,
          std::bind(&http_session::loop, shared_from_this(),
                    std::placeholders::_1, std::placeholders::_2, false));

      if (ec == http::error::end_of_stream) {
        break;
      } else if (ec == boost::asio::error::operation_aborted) {
        return;
      } else if (ec) {
        return fail(ec, "http read");
      } else if (boost::beast::websocket::is_upgrade(req_)) {
        auto websocket = std::make_shared<ws_session>(
            session_mgr_, std::move(stream_), ws_msg_cb_, ws_close_cb_);
        if (ws_open_cb_) {
          asio::post(
              websocket->ws_.get_executor(),
              [cb = &ws_open_cb_, websocket, this]() { (*cb)(websocket); });
        }
        websocket->run(std::move(req_));
        return;
      }

      if (http_req_cb_) {
        yield http_req_cb_(std::move(req_), [this, self = shared_from_this()](
                                                web_server::http_res_t res) {
          res_ = std::move(res);
          asio::post(boost::beast::get_lowest_layer(stream_).get_executor(),
                     std::bind(&http_session::loop, self,
                               boost::system::error_code{}, 0, false));
        });
      } else {
        res_ = not_found(req_);
      }

      yield http::async_write(
          stream_, res_,
          std::bind(&http_session::loop, shared_from_this(),
                    std::placeholders::_1, std::placeholders::_2,
                    res_.need_eof()));
      if (ec) {
        return fail(ec, "http write");
      }
      if (close) {
        break;
      }
      res_ = {};
    }

    yield stream_.async_shutdown(std::bind(&http_session::loop,
                                           shared_from_this(),
                                           std::placeholders::_1, 0, false));
    if (ec) {
      return fail(ec, "http shutdown");
    }
  }
}
#include "boost/asio/unyield.hpp"

}  // namespace net