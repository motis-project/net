#ifndef NET_TCP_SERVER_H_
#define NET_TCP_SERVER_H_

#include <memory>
#include <utility>
#include <functional>
#include <vector>
#include <istream>

#include "boost/asio.hpp"

namespace net {

typedef std::function<void(std::string const&, bool)> handler_cb_fun;
typedef std::function<void(std::string const&, handler_cb_fun)> handler_fun;

class tcp_server : public std::enable_shared_from_this<tcp_server> {
  class client : public std::enable_shared_from_this<client>,
                 public boost::asio::coroutine {
  public:
    client(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
           std::shared_ptr<tcp_server> server, handler_fun& handler)
        : socket_(socket), server_(server), handler_(handler) {}

    void start() { handle(this->shared_from_this()); }

    void stop() { socket_->close(); }

  private:
#include "boost/asio/yield.hpp"
    void handle(std::shared_ptr<client> self, std::string const& response = "",
                bool close = false,
                boost::system::error_code ec = boost::system::error_code(),
                std::size_t bytes_transferred = 0) {
      (void)(bytes_transferred);

      if (ec && ec != boost::asio::error::eof) {
        return;
      }

      using namespace std::placeholders;
      auto re = std::bind(&client::handle, this, self, "", false, _1, _2);
      auto re1 = std::bind(&client::handle, this, self, _1, _2,
                           boost::system::error_code(), 0);
      boost::system::error_code ignore;

      reenter(this) {
        using namespace boost::asio;

        while (true) {
          yield async_read(*socket_, read_buf_, transfer_all(), re);
          handler_({boost::asio::buffer_cast<const char*>(read_buf_.data()),
                    read_buf_.size()},
                   re1);
          yield async_write(*socket_, buffer(response), re);

          if (close) {
            socket_->shutdown(ip::tcp::socket::shutdown_both, ignore);
            break;
          }
        }
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
  tcp_server(boost::asio::io_service& ios)
      : stopped_(false), ios_(ios), acceptor_(ios) {}

  void listen(std::string const& address, std::string const& port,
              handler_fun request_handler) {
    handler_ = request_handler;
    boost::asio::ip::tcp::resolver resolver(ios_);
    boost::asio::ip::tcp::endpoint endpoint =
        *resolver.resolve({address, port});
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    do_accept();
  }

  void do_accept() {
    if (stopped_) {
      return;
    }

    cleanup_clients();
    auto self = this->shared_from_this();
    auto con = std::make_shared<boost::asio::ip::tcp::socket>(ios_);
    acceptor_.async_accept(
        *con, [this, con, self](boost::system::error_code const& ec) {
          if (!ec) {
            auto c = std::make_shared<client>(con, self, handler_);
            c->start();
            clients_.emplace_back(c);
          }
          do_accept();
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

std::shared_ptr<tcp_server> make_server(boost::asio::io_service& ios) {
  return std::make_shared<tcp_server>(ios);
}

}  // namespace net

#endif  // NET_TCP_SERVER_H_
