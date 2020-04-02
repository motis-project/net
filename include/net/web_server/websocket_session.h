#pragma once

#include "boost/beast/core/tcp_stream.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/string_body.hpp"

#if defined(NET_TLS)
#include "boost/beast/ssl.hpp"
#endif

#include "net/web_server/web_server.h"
#include "net/web_server/web_server_settings.h"

namespace net {

void make_websocket_session(
    boost::beast::tcp_stream stream,
    boost::beast::http::request<boost::beast::http::string_body> req,
    web_server_settings const& settings);

#if defined(NET_TLS)
void make_websocket_session(
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
    boost::beast::http::request<boost::beast::http::string_body> req,
    web_server_settings const& settings);
#endif

}  // namespace net
