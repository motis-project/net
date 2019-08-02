#pragma once

#include <memory>
#include <queue>
#include <string>

#include "boost/asio/coroutine.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl/context.hpp"
#include "boost/beast/core/multi_buffer.hpp"
#include "boost/beast/ssl/ssl_stream.hpp"
#include "boost/beast/websocket/stream.hpp"

#include "net/web_server/session_manager.h"
#include "net/web_server/web_server.h"

namespace net {

struct ws_session : public session,
                    public std::enable_shared_from_this<ws_session>,
                    public boost::asio::coroutine {
  using send_cb_t = std::function<void(boost::system::error_code, size_t)>;

  ws_session(session_manager& session_mgr,
             boost::beast::ssl_stream<boost::asio::ip::tcp::socket> stream,
             web_server::ws_msg_cb_t& ws_msg_cb,
             web_server::ws_close_cb_t& ws_close_cb);

  ws_session(ws_session const&) = delete;
  ws_session(ws_session&&) = delete;

  ws_session& operator=(ws_session const&) = delete;
  ws_session& operator=(ws_session&&) = delete;

  ~ws_session() override;

  void stop() override;
  void run(web_server::http_req_t const& update_req);
  void loop(boost::system::error_code ec, std::size_t /* bytes_transferred */);

  session_manager& session_mgr_;

  boost::beast::websocket::stream<
      boost::beast::ssl_stream<boost::asio::ip::tcp::socket>>
      ws_;
  boost::beast::multi_buffer buffer_;

  web_server::ws_msg_cb_t& ws_msg_cb_;
  web_server::ws_close_cb_t& ws_close_cb_;

  std::queue<std::tuple<std::string, ws_msg_type, send_cb_t>> send_queue_;
  bool send_active_{false};
};

}  // namespace net