#pragma once

#include <chrono>

#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl/context.hpp"

#include "net/web_server/web_server.h"
#include "net/web_server/web_server_settings.h"

namespace net {

#if defined(NET_TLS)
void make_detect_session(boost::asio::ip::tcp::socket&& socket,
                         boost::asio::ssl::context& ctx,
                         web_server_settings const& settings);
#else
void make_detect_session(boost::asio::ip::tcp::socket&& socket,
                         web_server_settings const& settings);
#endif

}  // namespace net
