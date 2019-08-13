#pragma once

#include <chrono>

#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl/context.hpp"

#include "net/web_server/web_server.h"

namespace net {

void make_detect_session(boost::asio::ip::tcp::socket&& socket,
                         boost::asio::ssl::context& ctx,
                         web_server::http_req_cb_t& http_req_cb,
                         web_server::ws_msg_cb_t& ws_msg_cb,
                         web_server::ws_open_cb_t& ws_open_cb,
                         web_server::ws_close_cb_t& ws_close_cb,
                         std::chrono::nanoseconds const& timeout);

}  // namespace net
