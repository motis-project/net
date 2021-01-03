#pragma once

#include <functional>
#include <memory>
#include <string>

#include "boost/system/error_code.hpp"

#include "boost/asio/coroutine.hpp"
#include "boost/asio/io_service.hpp"
#include "boost/asio/ssl/context.hpp"

namespace net {

struct wss_client {
  wss_client(boost::asio::io_service& ios, boost::asio::ssl::context& ctx,
             std::string const& host, std::string const& port);
  ~wss_client();

  void run(const std::function<void(boost::system::error_code)>&) const;
  void send(std::string const&, bool binary) const;
  void on_msg(std::function<void(std::string, bool /* binary */)>) const;
  void on_fail(std::function<void(boost::system::error_code)>) const;
  void stop();

  struct impl;
  std::shared_ptr<impl> impl_;
};

}  // namespace net