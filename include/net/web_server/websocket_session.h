#pragma once

#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/string_body.hpp"
#include "boost/beast/ssl.hpp"

#include "net/web_server/web_server.h"

namespace net {

void make_websocket_session(
    boost::beast::tcp_stream stream,
    boost::beast::http::request<boost::beast::http::string_body> req,
    web_server::ws_msg_cb_t& ws_msg_cb, web_server::ws_open_cb_t& ws_open_cb,
    web_server::ws_close_cb_t& ws_close_cb);

void make_websocket_session(
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
    boost::beast::http::request<boost::beast::http::string_body> req,
    web_server::ws_msg_cb_t& ws_msg_cb, web_server::ws_open_cb_t& ws_open_cb,
    web_server::ws_close_cb_t& ws_close_cb);

}  // namespace net
