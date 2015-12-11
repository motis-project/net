//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include "net/http/server/server.hpp"

#include <signal.h>
#include <utility>

namespace net {
namespace http {
namespace server {

server::server(boost::asio::io_service& io_service,
               std::string const& address, std::string const& port,
               request_handler request_handler)
  : io_service_(io_service),
    acceptor_(io_service_),
    connection_manager_(),
    socket_(io_service_)
{
  listen(address, port, std::move(request_handler));
}

server::server(boost::asio::io_service& io_service)
    : io_service_(io_service),
      acceptor_(io_service_),
      connection_manager_(),
      socket_(io_service_)
{}

void server::listen(std::string const& address, std::string const& port,
                    request_handler request_handler) {
  // Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
  request_handler_ = request_handler;
  boost::asio::ip::tcp::resolver resolver(io_service_);
  boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve({address, port});
  acceptor_.open(endpoint.protocol());
  acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
  acceptor_.bind(endpoint);
  acceptor_.listen();
  do_accept();
}

void server::do_accept()
{
  acceptor_.async_accept(socket_,
      [this](boost::system::error_code ec)
      {
        // Check whether the server was stopped by a signal before this
        // completion handler had a chance to run.
        if (!acceptor_.is_open())
        {
          return;
        }

        if (!ec)
        {
          connection_manager_.start(std::make_shared<connection>(
              std::move(socket_), connection_manager_, request_handler_));
        }

        do_accept();
      });
}

void server::stop()
{
  acceptor_.close();
  connection_manager_.stop_all();
}

} // namespace server
} // namespace http
} // namespace net
