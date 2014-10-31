#ifndef NET_TCP_SERVER_H_
#define NET_TCP_SERVER_H_

#include <memory>
#include <functional>
#include <vector>
#include <istream>

#include "boost/asio.hpp"

namespace net {

typedef std::function<std::string (std::string)> handler_fun;

class tcp_server : public std::enable_shared_from_this<tcp_server> {
  class client : public std::enable_shared_from_this<client>,
                 public boost::asio::coroutine {
  public:
    client(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
           std::shared_ptr<tcp_server> server,
           handler_fun& handler)
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
        write_buf_ = handler_({
            boost::asio::buffer_cast<const char*>(read_buf_.data()),
            read_buf_.size()
        });
        yield async_write(*socket_, buffer(write_buf_), re);
        socket_->shutdown(ip::tcp::socket::shutdown_both, ignore);
      }
    }
#include "boost/asio/unyield.hpp"

    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::shared_ptr<tcp_server> server_;
    handler_fun& handler_;

    boost::asio::streambuf read_buf_;
    std::string write_buf_;
  };

public:
  tcp_server(boost::asio::io_service& ios,
             uint16_t port,
             handler_fun handler)
      : stopped_(false),
        ios_(ios),
        acceptor_(ios, boost::asio::ip::tcp::endpoint(
                           boost::asio::ip::tcp::v4(), port)),
        handler_(std::forward<handler_fun>(handler)) {
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
  handler_fun handler_;
  std::vector<std::weak_ptr<client>> clients_;
};

std::shared_ptr<tcp_server> make_server(
    boost::asio::io_service& ios, uint16_t port, handler_fun handler) {
  return std::make_shared<tcp_server>(ios, port, std::move(handler));
}

}  // namespace net

#endif  // NET_TCP_SERVER_H_
