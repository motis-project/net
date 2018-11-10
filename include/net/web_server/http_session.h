#pragma once

#include <memory>

#include "boost/asio/coroutine.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl/context.hpp"
#include "boost/beast/core/flat_buffer.hpp"

#include "net/web_server/session_manager.h"
#include "net/web_server/ssl_stream.h"
#include "net/web_server/web_server.h"

namespace net {

struct http_session : public session,
                      public std::enable_shared_from_this<http_session>,
                      public boost::asio::coroutine {
  http_session(session_manager& session_mgr,
               boost::asio::ip::tcp::socket socket,
               boost::asio::ssl::context& ctx,
               web_server::http_req_cb_t& http_req_cb,
               web_server::ws_msg_cb_t& ws_msg_cb,
               web_server::ws_open_cb_t& ws_open_cb,
               web_server::ws_close_cb_t& ws_close_cb);
  ~http_session() override;

  http_session(http_session const&) = delete;
  http_session(http_session&&) = delete;

  http_session& operator=(http_session const&) = delete;
  http_session& operator=(http_session&&) = delete;

  void run();
  void stop() override;

private:
  void loop(boost::system::error_code ec, std::size_t bytes_transferred,
            bool close);

  session_manager& session_mgr_;

  ssl_stream<boost::asio::ip::tcp::socket> stream_;

  boost::beast::flat_buffer buffer_;

  web_server::http_req_t req_;
  web_server::http_res_t res_;

  web_server::http_req_cb_t& http_req_cb_;
  web_server::ws_msg_cb_t& ws_msg_cb_;
  web_server::ws_open_cb_t& ws_open_cb_;
  web_server::ws_close_cb_t& ws_close_cb_;
};

}  // namespace net