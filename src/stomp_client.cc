#include "net/stomp_client.h"

#include <cctype>
#include <iostream>

using boost::system::error_code;
namespace asio = boost::asio;

namespace net {

stomp_client::stomp_client(boost::asio::io_service& ios, std::string host,
                           std::string port, std::string destination)
    : net::tcp(ios, std::move(host), std::move(port),
               boost::posix_time::seconds(10)),
      destination_(std::move(destination)),
      beat_timer_(ios),
      beat_timeout_timer_(ios) {
  init_commands();
}

void stomp_client::init_commands() {
  connect_cmd_ = std::string(
      "CONNECT\r\n"
      "login:a\r\n"
      "passcode:b\r\n"
      "heart-beat:5000,1000\r\n\r\n");
  connect_cmd_ += '\0';

  subscribe_cmd_ = std::string(
                       "SUBSCRIBE\r\n"
                       "destination:") +
                   destination_ +
                   "\r\n"
                   "ack:auto\r\n\r\n";
  subscribe_cmd_ += '\0';

  beat_cmd_ = "\r\n";
  beat_cmd_ += '\0';
}

void stomp_client::subscribe(callback cb,
                             std::function<void(std::string)> on_msg) {
  return connect([this, cb, on_msg](net::tcp::tcp_ptr self, error_code ec) {
    if (ec) {
      return cb(self, ec);
    } else {
      return transfer(self, std::move(cb), std::move(on_msg), ec);
    }
  });
}

#include "boost/asio/yield.hpp"
void stomp_client::transfer(net::tcp::tcp_ptr self, callback cb,
                            std::function<void(std::string)> on_msg,
                            error_code ec) {
  if (ec) {
    asio::detail::coroutine_ref(this) = 0;
    return respond(cb, self, ec);
  }

  if (is_complete()) {
    asio::detail::coroutine_ref(this) = 0;
  }

  using std::placeholders::_1;
  auto re =
      std::bind(&stomp_client::transfer, this, self, std::move(cb), on_msg, _1);
  error_code ignored;

  reenter(this) {
    yield asio::async_write(socket_, asio::buffer(connect_cmd_), re);
    yield asio::async_read(socket_, buf_, asio::transfer_at_least(1), re);
    req_timeout_timer_.cancel();
    buf_.consume(buf_.size());

    heartbeat(self, error_code());

    yield asio::async_write(socket_, asio::buffer(subscribe_cmd_), re);

    while (true) {
      yield asio::async_read(socket_, buf_, asio::transfer_at_least(1), re);
      push_server_timeout();
      skip_whitespace();

      if (buf_.size() != 0) {
        yield asio::async_read_until(socket_, buf_, '\0', re);

        auto buf = boost::asio::buffer_cast<const char*>(buf_.data());
        on_msg(std::string(buf, buf_.size()));
        buf_.consume(buf_.size());
      }
    }
  }
}
#include "boost/asio/unyield.hpp"

void stomp_client::skip_whitespace() {
  while (buf_.size() != 0) {
    const char* buf = boost::asio::buffer_cast<const char*>(buf_.data());
    if (!std::isspace(buf[0])) {
      break;
    }
    buf_.consume(1);
  }
}

void stomp_client::push_server_timeout() {
  beat_timeout_timer_.expires_from_now(boost::posix_time::seconds(5));
  beat_timeout_timer_.async_wait(std::bind(&stomp_client::server_timeout, this,
                                           shared_from_this(),
                                           std::placeholders::_1));
}

void stomp_client::server_timeout(net::tcp::tcp_ptr, error_code ec) {
  if (ec == boost::asio::error::operation_aborted) {
    return;
  } else {
    cancel();
  }
}

void stomp_client::heartbeat(net::tcp::tcp_ptr self, error_code ec) {
  if (ec == boost::asio::error::operation_aborted) {
    return;
  }

  asio::async_write(
      socket_, asio::buffer(beat_cmd_),
      [self, this](error_code ec, std::size_t /* bytes_transferred */) {
        if (ec) {
          std::cout << ec.message() << "\n";
          cancel();
        }
      });

  beat_timer_.expires_from_now(boost::posix_time::seconds(5));
  beat_timer_.async_wait(std::bind(&stomp_client::heartbeat, this,
                                   shared_from_this(), std::placeholders::_1));
}

void stomp_client::respond(callback cb, net::tcp::tcp_ptr self, error_code ec) {
  finally(ec);
  if (asio::error::eof == ec) {
    ec = error_code();
  }
  cb(self, ec);
}

void stomp_client::cancel() {
  tcp::cancel();
  beat_timer_.cancel();
  beat_timeout_timer_.cancel();
}

}  // namespace net