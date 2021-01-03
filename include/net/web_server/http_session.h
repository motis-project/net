#pragma once

#include <chrono>

#include "boost/beast/core/flat_buffer.hpp"
#include "boost/beast/core/tcp_stream.hpp"

#if defined(NET_TLS)
#include "boost/beast/ssl.hpp"
#endif

#include "net/web_server/web_server.h"
#include "net/web_server/web_server_settings.h"

namespace net {

void make_http_session(boost::beast::tcp_stream&& stream,
                       boost::beast::flat_buffer&& buffer,
                       web_server_settings_ptr const& settings);

#if defined(NET_TLS)
void make_http_session(boost::beast::tcp_stream&& stream,
                       boost::asio::ssl::context& ctx,
                       boost::beast::flat_buffer&& buffer,
                       web_server_settings_ptr const& settings);
#endif

}  // namespace net
