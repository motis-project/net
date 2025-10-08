#ifndef NET_TCP_SERVER_H_
#define NET_TCP_SERVER_H_

#include <functional>
#include <istream>
#include <memory>
#include <utility>
#include <vector>

#include "boost/array.hpp"
#include "boost/asio.hpp"

namespace net {

typedef std::function<void(std::string const&, bool)> handler_cb_fun;
typedef std::function<void(std::string const&, handler_cb_fun)> handler_fun;

class tcp_server : public std::enable_shared_from_this<tcp_server> {
  typedef uint32_t msg_size_t;

  class client : public std::enable_shared_from_this<client>,
                 public boost::asio::coroutine {
  public:
    client(std::shared_ptr<boost::asio::ip::tcp::socket> socket,
           std::shared_ptr<tcp_server> server, handler_fun& handler)
        : socket_(socket), server_(server), handler_(handler) {}

    void start() {
      handle(this->shared_from_this(), "", false, boost::system::error_code(),
             0);
    }

    void stop() { socket_->close(); }

  private:
#include "boost/asio/yield.hpp"
    void handle(std::shared_ptr<client> self, std::string const& response,
                bool close, boost::system::error_code ec,
                std::size_t /* bytes_transferred */) {
      close = ec == boost::asio::error::eof;
      if (ec) {
        return;
      }

      using namespace std::placeholders;
      auto re = std::bind(&client::handle, this, self, "", false, _1, _2);
      auto re1 = std::bind(&client::handle, this, self, _1, _2,
                           boost::system::error_code(), 0);
      boost::system::error_code ignore;
      msg_size_t req_size, res_size;

      reenter(this) {
        using namespace boost::asio;

        while (true) {
          // Receive request size.
          yield async_read(*socket_, buffer(size_buf_),
                           transfer_exactly(sizeof(uint32_t)), re);
          req_size = ntohl(*reinterpret_cast<msg_size_t*>(size_buf_.data()));

          // Read request.
          yield async_read(*socket_, read_buf_, transfer_exactly(req_size), re);

          // Handle request.
          yield handler_(
              {boost::asio::buffer_cast<const char*>(read_buf_.data()),
               read_buf_.size()},
              re1);

          // Write response.
          res_size = htonl(static_cast<msg_size_t>(response.size()));
          write_buf_ = response;
          yield async_write(
              *socket_,
              buffer(reinterpret_cast<void*>(&res_size), sizeof(res_size)), re);
          yield async_write(*socket_, buffer(write_buf_), transfer_all(), re);

          if (close) {
            socket_->shutdown(ip::tcp::socket::shutdown_both, ignore);
            return;
          } else {
            reset();
          }
        }
      }
    }
#include "boost/asio/unyield.hpp"

    void reset() {
      std::fill(std::begin(size_buf_), std::end(size_buf_), 0);
      write_buf_.clear();
      read_buf_.consume(read_buf_.size());
    }

    std::shared_ptr<boost::asio::ip::tcp::socket> socket_;
    std::shared_ptr<tcp_server> server_;
    handler_fun& handler_;

    boost::array<unsigned char, sizeof(msg_size_t)> size_buf_;
    boost::asio::streambuf read_buf_;
    std::string write_buf_;
  };

public:
  tcp_server(boost::asio::io_context& ios)
      : active_(false), ios_(ios), acceptor_(ios) {}

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
    active_ = true;
    do_accept();
  }

  void do_accept() {
    if (!active_) {
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
    if (!active_) {
      return;
    }

    active_ = false;
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

  bool active_;
  boost::asio::io_context& ios_;
  boost::asio::ip::tcp::acceptor acceptor_;
  handler_fun handler_;
  std::vector<std::weak_ptr<client>> clients_;
};

std::shared_ptr<tcp_server> make_server(boost::asio::io_context& ios) {
  return std::make_shared<tcp_server>(ios);
}

}  // namespace net

#endif  // NET_TCP_SERVER_H_
