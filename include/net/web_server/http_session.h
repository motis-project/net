#pragma once

#include <chrono>

#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/ssl.hpp"

#include "net/web_server/web_server.h"

namespace net {

void make_http_session(boost::beast::tcp_stream&& stream,
                       boost::beast::flat_buffer&& buffer,
                       web_server::http_req_cb_t& http_req_cb,
                       web_server::ws_msg_cb_t& ws_msg_cb,
                       web_server::ws_open_cb_t& ws_open_cb,
                       web_server::ws_close_cb_t& ws_close_cb,
                       std::chrono::nanoseconds const& timeout);

void make_http_session(boost::beast::tcp_stream&& stream,
                       boost::asio::ssl::context& ctx,
                       boost::beast::flat_buffer&& buffer,
                       web_server::http_req_cb_t& http_req_cb,
                       web_server::ws_msg_cb_t& ws_msg_cb,
                       web_server::ws_open_cb_t& ws_open_cb,
                       web_server::ws_close_cb_t& ws_close_cb,
                       std::chrono::nanoseconds const& timeout);

}  // namespace net
