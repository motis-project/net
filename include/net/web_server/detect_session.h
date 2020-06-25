#pragma once

#include <chrono>

#include "boost/asio/ip/tcp.hpp"

#if defined(NET_TLS)
#include "boost/asio/ssl/context.hpp"
#endif

#include "net/web_server/web_server.h"
#include "net/web_server/web_server_settings.h"

namespace net {

#if defined(NET_TLS)
void make_detect_session(boost::asio::ip::tcp::socket&& socket,
                         boost::asio::ssl::context& ctx,
                         web_server_settings_ptr settings);
#else
void make_detect_session(boost::asio::ip::tcp::socket&& socket,
                         web_server_settings_ptr settings);
#endif

}  // namespace net
