#include "net/web_server/ws_session.h"

#include <iostream>

#include "boost/asio/post.hpp"
#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/websocket.hpp"
#include "boost/beast/websocket/ssl.hpp"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
namespace websocket = boost::beast::websocket;
using tcp = asio::ip::tcp;

namespace net {

ws_session::ws_session(session_manager& session_mgr,
                       ssl_stream<tcp::socket> stream,  //
                       web_server::ws_msg_cb_t& ws_msg_cb,
                       web_server::ws_close_cb_t& ws_close_cb)
    : session_mgr_{session_mgr},
      ws_{std::move(stream)},
      ws_msg_cb_{ws_msg_cb},
      ws_close_cb_{ws_close_cb} {
  is_websocket_ = true;
  session_mgr_.add(this);
}

ws_session::~ws_session() {
  session_mgr_.remove(this);
  if (ws_close_cb_) {
    ws_close_cb_(this);
  }
}

void ws_session::stop() { boost::beast::get_lowest_layer(ws_).close(); }

void ws_session::run(web_server::http_req_t const& update_req) {
  ws_.async_accept(update_req, std::bind(&ws_session::loop, shared_from_this(),
                                         std::placeholders::_1, 0));
}

#include "boost/asio/yield.hpp"
void ws_session::loop(boost::system::error_code ec,
                      std::size_t /* bytes_transferred */) {
  reenter(*this) {
    if (ec) {
      return;
    }

    while (true) {
      yield ws_.async_read(
          buffer_, std::bind(&ws_session::loop, shared_from_this(),
                             std::placeholders::_1, std::placeholders::_2));
      if (ec == websocket::error::closed ||
          ec == boost::asio::error::operation_aborted) {
        return;
      }
      if (ec) {
        stop();
        return;
      }

      if (ws_msg_cb_) {
        ws_msg_cb_(shared_from_this(),
                   boost::beast::buffers_to_string(buffer_.data()),
                   ws_.got_text() ? ws_msg_type::TEXT : ws_msg_type::BINARY);
      }

      buffer_.consume(buffer_.size());
    }
  }
}
#include "boost/asio/unyield.hpp"

void send_next(std::shared_ptr<ws_session> const& s) {
  if (s->send_active_) {
    return;
  }

  if (s->send_queue_.empty()) {
    s->send_active_ = false;
    return;
  }

  std::string msg;
  ws_msg_type type;
  ws_session::send_cb_t cb;
  std::tie(msg, type, cb) = s->send_queue_.front();
  s->send_queue_.pop();

  auto m = std::make_shared<std::string>(std::move(msg));
  s->ws_.text(type == ws_msg_type::TEXT);
  s->ws_.binary(type == ws_msg_type::BINARY);
  s->ws_.async_write(
      boost::asio::buffer(m->data(), m->size()),
      [s, m, cb, self = s->shared_from_this()](
          boost::system::error_code const& ec, size_t bytes_transferred) {
        s->send_active_ = false;
        send_next(s);
        asio::post(s->ws_.get_executor(), [s, cb, ec, bytes_transferred]() {
          cb(ec, bytes_transferred);
        });
      });
  s->send_active_ = true;
}

void ws_send(std::shared_ptr<ws_session> const& s, std::string msg,
             ws_msg_type type,
             std::function<void(boost::system::error_code, size_t)> cb) {
  if (s == nullptr) {
    cb(boost::asio::error::make_error_code(boost::asio::error::shut_down), 0);
    return;
  }

  s->send_queue_.emplace(msg, type, cb);
  send_next(s);
}

void ws_broadcast(std::set<session*> const& sessions, std::string msg,
                  ws_msg_type type,
                  std::function<void(boost::system::error_code, size_t)> cb) {
  for (auto s : sessions) {
    if (s->is_websocket_) {
      ws_send(reinterpret_cast<ws_session*>(s)->shared_from_this(), msg, type,
              cb);
    }
  }
}

void ws_broadcast(session_manager const* session_mgr, std::string msg,
                  ws_msg_type type,
                  std::function<void(boost::system::error_code, size_t)> cb) {
  ws_broadcast(session_mgr->sessions_, std::move(msg), type, std::move(cb));
}

}  // namespace net