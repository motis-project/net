#ifndef NET_TCP_SERVER_H_
#define NET_TCP_SERVER_H_

#include <memory>
#include <functional>
#include <vector>
#include <istream>

#include "boost/asio.hpp"

namespace net {

template<typename HandlerFun>
class tcp_server : public std::enable_shared_from_this<tcp_server<HandlerFun>> {
  class client : public std::enable_shared_from_this<client>,
                 public boost::asio::coroutine {
  public:
    client(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
           std::shared_ptr<tcp_server> server,
           HandlerFun& handler)
        : socket_(socket),
          server_(server),
          handler_(handler) {
    }

    void start() {
      handle(this->shared_from_this());
    }

    void stop() {
      socket_->close();
    }

  private:
#include "boost/asio/yield.hpp"
    void handle(std::shared_ptr<client> self,
                boost::system::error_code ec = boost::system::error_code(),
                std::size_t bytes_transferred = 0) {
      if (ec && ec != boost::asio::error::eof) {
        return;
      }

      using namespace std::placeholders;
      auto re = std::bind(&client::handle, this, self, _1, _2);
      boost::system::error_code ignore;

      reenter (this) {
        using namespace boost::asio;
        yield async_read(*socket_, read_buf_, transfer_all(), re);
        write_buf_ = handler_(input());
        yield async_write(*socket_, buffer(write_buf_), re);
        socket_->shutdown(ip::tcp::socket::shutdown_both, ignore);
      }
    }
#include "boost/asio/unyield.hpp"

    std::string input() {
      std::istream in(&read_buf_);
      std::string s;
      in >> s;
      return s;
    }

    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::shared_ptr<tcp_server> server_;
    HandlerFun& handler_;

    boost::asio::streambuf read_buf_;
    std::string write_buf_;
  };

public:
  tcp_server(boost::asio::io_service& ios,
             uint16_t port,
             HandlerFun handler)
      : stopped_(false),
        ios_(ios),
        acceptor_(ios, boost::asio::ip::tcp::endpoint(
                           boost::asio::ip::tcp::v4(), port)),
        handler_(std::forward<HandlerFun>(handler)) {
  }

  void start() {
    if (stopped_) {
      return;
    }

    cleanup_clients();
    auto self = this->shared_from_this();
    auto con = std::make_shared<boost::asio::ip::tcp::socket>(ios_);
    acceptor_.async_accept(
      *con,
      [this, con, self](boost::system::error_code const& ec) {
        if (!ec) {
          auto c = std::make_shared<client>(con, self, handler_);
          c->start();
          clients_.emplace_back(c);
        }
        start();
      });
  }

  void stop() {
    stopped_ = true;
    acceptor_.cancel();

    for (auto& client : clients_) {
      auto lock = client.lock();
      if (lock) {
        lock->stop();
      }
    }
  }

private:
  void cleanup_clients() {
    for (auto it = begin(clients_); it != end(clients_);) {
      auto lock = it->lock();
      if (lock) {
        it = clients_.erase(it);
      } else {
        ++it;
      }
    }
  }

  bool stopped_;
  boost::asio::io_service& ios_;
  boost::asio::ip::tcp::acceptor acceptor_;
  HandlerFun handler_;
  std::vector<std::weak_ptr<client>> clients_;
};

template<typename HandlerFun>
std::shared_ptr<tcp_server<HandlerFun>> make_server(
    boost::asio::io_service& ios, uint16_t port, HandlerFun handler) {
  return std::make_shared<tcp_server<HandlerFun>>(
      ios, port, std::forward<HandlerFun>(handler));
}

}  // namespace net

#endif  // NET_TCP_SERVER_H_
