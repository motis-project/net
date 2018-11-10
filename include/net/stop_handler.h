#pragma once

#include <functional>

#include "boost/asio/io_service.hpp"
#include "boost/asio/signal_set.hpp"

namespace net {

struct stop_handler {
  stop_handler(boost::asio::io_service& ios, std::function<void()> fn)
      : fn_(std::move(fn)), signals_(ios) {
    signals_.add(SIGINT);
    signals_.add(SIGTERM);
    signals_.async_wait([this](boost::system::error_code, int) { fn_(); });
  }

  void stop() {
    if (fn_) {
      fn_();
    }
  }

private:
  std::function<void()> fn_;
  boost::asio::signal_set signals_;
};

}  // namespace net