#include <fstream>
#include <iostream>
#include <thread>

#include "boost/asio/io_service.hpp"
#include "boost/asio/ssl.hpp"

#include "net/wss_client.h"

using net::wss_client;

std::string req(int i) {
  return std::string{R"({
  "destination": {
    "type": "Module",
    "target": "/guesser"
  },
  "content_type": "StationGuesserRequest",
  "content": {
    "input": "Darmst",
    "guess_count": 10
  },
  "id":)"} +
         std::to_string(i) + "}";
}

int main(int argc, char** argv) {
  if (argc != 4) {
    printf("usage: %s host port num_requests\n", argv[0]);
    return 0;
  }

  boost::asio::io_service ios;

  boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23};
  boost::system::error_code ignore;
  ctx.set_verify_mode(boost::asio::ssl::verify_none, ignore);

  auto const num_requests = std::stoi(argv[3]);
  auto response_count = 0;

  std::unique_ptr<wss_client> c;
  std::function<void()> init = [&]() {
    if (response_count >= num_requests) {
      return;
    }
    c = std::make_unique<wss_client>(ios, ctx, argv[1], argv[2]);
    std::cout << "connecting ...\n";
    c->run([&](boost::system::error_code ec) {
      if (ec) {
        std::cout << "connect failed: " << ec.message() << "\n";
        return;
      }
      std::cout << "connected\n";
      for (auto i = 1; i <= num_requests; ++i) {
        c->send(req(i), false);
      }
    });
    c->on_fail([&](boost::system::error_code ec) {
      std::cout << "restart\n";
      init();
    });
    c->on_msg([&](std::string const& msg, bool /* binary */) {
      ++response_count;
      printf("~~~ received %zu: [%s]\n", msg.size(), msg.c_str());
      if (response_count >= num_requests) {
        c->stop();
      }
    });
  };
  init();

  ios.run();
}