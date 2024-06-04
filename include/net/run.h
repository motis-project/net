#pragma once

#include <iostream>

#include "boost/asio/io_context.hpp"

namespace net {

auto run(boost::asio::io_context& ioc) {
  return [&ioc]() {
    while (true) {
      try {
        ioc.run();
        break;
      } catch (std::exception const& e) {
        std::cerr << "unhandled error: " << e.what() << "\n";
      } catch (...) {
        std::cerr << "unhandled unknown error\n";
      }
    }
  };
}

}  // namespace net