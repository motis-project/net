#pragma once

#include <memory>

#include "boost/asio/coroutine.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl/context.hpp"
#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/tcp_stream.hpp"

#include "net/web_server/session_manager.h"
#include "net/web_server/web_server.h"

namespace net {

struct detect_session : public session,
                        public std::enable_shared_from_this<detect_session>,
                        public boost::asio::coroutine {
  detect_session(session_manager& session_mgr,
                 boost::asio::ip::tcp::socket socket,
                 boost::asio::ssl::context& ctx,
                 web_server::http_req_cb_t& http_req_cb,
                 web_server::ws_msg_cb_t& ws_msg_cb,
                 web_server::ws_open_cb_t& ws_open_cb,
                 web_server::ws_close_cb_t& ws_close_cb);
  ~detect_session() override;

  detect_session(detect_session const&) = delete;
  detect_session(detect_session&&) = delete;

  detect_session& operator=(detect_session const&) = delete;
  detect_session& operator=(detect_session&&) = delete;

  void run();
  void stop() override;

private:
  void on_detect(boost::beast::error_code ec, bool result);

  session_manager& session_mgr_;

  // boost::beast::tcp_stream stream_;
  boost::asio::ip::tcp::socket stream_;
  boost::asio::ssl::context& ctx_;

  boost::beast::flat_buffer buffer_;

  web_server::http_req_t req_;
  web_server::http_res_t res_;

  web_server::http_req_cb_t& http_req_cb_;
  web_server::ws_msg_cb_t& ws_msg_cb_;
  web_server::ws_open_cb_t& ws_open_cb_;
  web_server::ws_close_cb_t& ws_close_cb_;
};

}  // namespace net