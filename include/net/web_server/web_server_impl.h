#pragma once

#include "boost/asio/bind_executor.hpp"
#include "boost/asio/signal_set.hpp"
#include "boost/asio/steady_timer.hpp"
#include "boost/asio/strand.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/ssl.hpp"
#include "boost/beast/version.hpp"
#include "boost/beast/websocket.hpp"
#include "boost/make_unique.hpp"
#include "boost/optional.hpp"

#include <cstdlib>
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "net/web_server/fail.h"
#include "net/web_server/web_server.h"

namespace net {

template <class Derived>
struct websocket_session : public ws_session {
  using send_cb_t = std::function<void(boost::system::error_code, size_t)>;

  websocket_session(web_server::ws_msg_cb_t& ws_msg_cb,
                    web_server::ws_open_cb_t& ws_open_cb,
                    web_server::ws_close_cb_t& ws_close_cb)
      : ws_msg_cb_(ws_msg_cb),
        ws_open_cb_(ws_open_cb),
        ws_close_cb_(ws_close_cb) {}

  ~websocket_session() {
    if (ws_close_cb_) {
      ws_close_cb_(this);
    }
  }

  // Start the asynchronous operation
  template <class Body, class Allocator>
  void run(boost::beast::http::request<
           Body, boost::beast::http::basic_fields<Allocator>>
               req) {
    // Accept the WebSocket upgrade request
    do_accept(std::move(req));
  }

  void send(std::string&& msg, ws_msg_type type, send_cb_t cb) override {
    send_queue_.emplace(std::move(msg), type, cb);
    send_next();
  }

  void send(std::string const& msg, ws_msg_type type, send_cb_t cb) override {
    send_queue_.emplace(msg, type, cb);
    send_next();
  }

private:
  // Start the asynchronous operation
  template <class Body, class Allocator>
  void do_accept(boost::beast::http::request<
                 Body, boost::beast::http::basic_fields<Allocator>>
                     req) {
    // Set suggested timeout settings for the websocket
    derived().ws().set_option(
        boost::beast::websocket::stream_base::timeout::suggested(
            boost::beast::role_type::server));

    // Set a decorator to change the Server of the handshake
    derived().ws().set_option(boost::beast::websocket::stream_base::decorator(
        [](boost::beast::websocket::response_type& res) {
          res.set(boost::beast::http::field::server,
                  std::string(BOOST_BEAST_VERSION_STRING));
        }));

    // Accept the websocket handshake
    derived().ws().async_accept(
        req, boost::beast::bind_front_handler(&websocket_session::on_accept,
                                              derived().shared_from_this()));
  }

  void on_accept(boost::beast::error_code ec) {
    if (ec) {
      return fail(ec, "accept");
    }

    if (ws_open_cb_) {
      boost::asio::post(derived().ws().get_executor(),
                        [&, self = derived().shared_from_this()] {
                          ws_open_cb_(self, derived().is_ssl());
                        });
    }

    // Read a message
    do_read();
  }

  void do_read() {
    // Read a message into our buffer
    derived().ws().async_read(buffer_, boost::beast::bind_front_handler(
                                           &websocket_session::on_read,
                                           derived().shared_from_this()));
  }

  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This indicates that the websocket_session was closed
    if (ec == boost::beast::websocket::error::closed) return;

    if (ec) {
      return fail(ec, "read");
    }

    if (ws_msg_cb_) {
      ws_msg_cb_(
          derived().shared_from_this(),
          boost::beast::buffers_to_string(buffer_.data()),
          derived().ws().got_text() ? ws_msg_type::TEXT : ws_msg_type::BINARY);
    }

    buffer_.consume(buffer_.size());
    do_read();
  }

  void on_write(boost::beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    if (ec) {
      return fail(ec, "write");
    }

    // Clear the buffer
    buffer_.consume(buffer_.size());

    // Do another read
    do_read();
  }

  void send_next() {
    if (send_active_ || send_queue_.empty()) {
      return;
    }

    std::string msg;
    ws_msg_type type;
    send_cb_t cb;
    std::tie(msg, type, cb) = send_queue_.front();
    send_queue_.pop();
    send_active_ = true;

    auto m = std::make_shared<std::string>(std::move(msg));
    derived().ws().text(type == ws_msg_type::TEXT);
    derived().ws().async_write(
        boost::asio::buffer(m->data(), m->size()),
        [m, cb, self = derived().shared_from_this()](
            boost::system::error_code const& ec, size_t bytes_transferred) {
          self->send_active_ = false;
          self->send_next();
          boost::asio::post(
              self->ws().get_executor(),
              [cb, ec, bytes_transferred]() { cb(ec, bytes_transferred); });
        });
  }

  // Access the derived class, this is part of
  // the Curiously Recurring Template Pattern idiom.
  Derived& derived() { return static_cast<Derived&>(*this); }

  boost::beast::flat_buffer buffer_;

  web_server::ws_msg_cb_t& ws_msg_cb_;
  web_server::ws_open_cb_t& ws_open_cb_;
  web_server::ws_close_cb_t& ws_close_cb_;

  std::queue<std::tuple<std::string, ws_msg_type, send_cb_t>> send_queue_;
  bool send_active_{false};
};

//------------------------------------------------------------------------------

// Handles a plain WebSocket connection
struct plain_websocket_session
    : public websocket_session<plain_websocket_session>,
      public std::enable_shared_from_this<plain_websocket_session> {
  // Create the session
  explicit plain_websocket_session(boost::beast::tcp_stream&& stream,
                                   web_server::ws_msg_cb_t& ws_msg_cb,
                                   web_server::ws_open_cb_t& ws_open_cb,
                                   web_server::ws_close_cb_t& ws_close_cb)
      : websocket_session<plain_websocket_session>(ws_msg_cb, ws_open_cb,
                                                   ws_close_cb),
        ws_(std::move(stream)) {}

  // Called by the base class
  boost::beast::websocket::stream<boost::beast::tcp_stream>& ws() {
    return ws_;
  }

  bool is_ssl() const { return false; }

private:
  boost::beast::websocket::stream<boost::beast::tcp_stream> ws_;
};

//------------------------------------------------------------------------------

// Handles an SSL WebSocket connection
struct ssl_websocket_session
    : public websocket_session<ssl_websocket_session>,
      public std::enable_shared_from_this<ssl_websocket_session> {
  // Create the ssl_websocket_session
  explicit ssl_websocket_session(
      boost::beast::ssl_stream<boost::beast::tcp_stream>&& stream,
      web_server::ws_msg_cb_t& ws_msg_cb, web_server::ws_open_cb_t& ws_open_cb,
      web_server::ws_close_cb_t& ws_close_cb)
      : websocket_session<ssl_websocket_session>(ws_msg_cb, ws_open_cb,
                                                 ws_close_cb),
        ws_(std::move(stream)) {}

  // Called by the base class
  boost::beast::websocket::stream<
      boost::beast::ssl_stream<boost::beast::tcp_stream>>&
  ws() {
    return ws_;
  }

  bool is_ssl() const { return true; }

private:
  boost::beast::websocket::stream<
      boost::beast::ssl_stream<boost::beast::tcp_stream>>
      ws_;
};

//------------------------------------------------------------------------------

template <class Body, class Allocator>
void make_websocket_session(
    boost::beast::tcp_stream stream,
    boost::beast::http::request<Body,
                                boost::beast::http::basic_fields<Allocator>>
        req,
    web_server::ws_msg_cb_t& ws_msg_cb, web_server::ws_open_cb_t& ws_open_cb,
    web_server::ws_close_cb_t& ws_close_cb) {
  std::make_shared<plain_websocket_session>(std::move(stream), ws_msg_cb,
                                            ws_open_cb, ws_close_cb)
      ->run(std::move(req));
}

template <class Body, class Allocator>
void make_websocket_session(
    boost::beast::ssl_stream<boost::beast::tcp_stream> stream,
    boost::beast::http::request<Body,
                                boost::beast::http::basic_fields<Allocator>>
        req,
    web_server::ws_msg_cb_t& ws_msg_cb, web_server::ws_open_cb_t& ws_open_cb,
    web_server::ws_close_cb_t& ws_close_cb) {
  std::make_shared<ssl_websocket_session>(std::move(stream), ws_msg_cb,
                                          ws_open_cb, ws_close_cb)
      ->run(std::move(req));
}

//------------------------------------------------------------------------------

boost::beast::http::response<boost::beast::http::string_body> not_found(
    web_server::http_req_t const& req) {
  boost::beast::http::response<boost::beast::http::string_body> res{
      boost::beast::http::status::not_found, req.version()};
  res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(boost::beast::http::field::content_type, "text/html");
  res.keep_alive(req.keep_alive());
  res.body() = "No handler implemented.";
  res.prepare_payload();
  return res;
}

//------------------------------------------------------------------------------

// Handles an HTTP server connection.
// This uses the Curiously Recurring Template Pattern so that
// the same code works with both SSL streams and regular sockets.
template <class Derived>
struct http_session {
  // Access the derived class, this is part of
  // the Curiously Recurring Template Pattern idiom.
  Derived& derived() { return static_cast<Derived&>(*this); }

  // This queue is used for HTTP pipelining.
  struct queue {
    enum {
      // Maximum number of responses we will queue
      limit = 8
    };

    // The type-erased, saved work item
    struct work {
      virtual ~work() = default;
      virtual void send() = 0;
    };

    explicit queue(http_session& self) : self_(self) {
      static_assert(limit > 0, "queue limit must be positive");
      items_.reserve(limit);
    }

    queue(queue const&) = delete;

    // Returns `true` if we have reached the queue limit
    bool is_full() const { return items_.size() >= limit; }

    // Called when a message finishes sending
    // Returns `true` if the caller should initiate a read
    bool on_write() {
      BOOST_ASSERT(!items_.empty());
      auto const was_full = is_full();
      items_.erase(items_.begin());
      return was_full;
    }

    bool send_next() {
      if (items_.empty() || !items_.front()->is_finished()) {
        return false;
      }
      self_.write_active_ = true;
      items_.front()->work_->send();
      return true;
    }

    struct queue_entry {
      queue_entry(http_session& session) : self_(session) {}

      bool is_finished() const { return static_cast<bool>(work_); }

      // Called by the HTTP handler to send a response.
      template <bool isRequest, class Body, class Fields>
      void operator()(
          boost::beast::http::message<isRequest, Body, Fields>&& msg) {
        // This holds a work item
        struct work_impl : work {
          http_session& self_;
          boost::beast::http::message<isRequest, Body, Fields> msg_;

          work_impl(http_session& self,
                    boost::beast::http::message<isRequest, Body, Fields>&& msg)
              : self_(self), msg_(std::move(msg)) {}

          void send() override {
            boost::beast::http::async_write(
                self_.derived().stream(), msg_,
                boost::beast::bind_front_handler(
                    &http_session::on_write, self_.derived().shared_from_this(),
                    msg_.need_eof()));
          }
        };

        work_ = std::make_unique<work_impl>(self_, std::move(msg));
        self_.send_next_response();
      }

      http_session& self_;
      std::unique_ptr<work> work_;
    };

    queue_entry& add_entry() {
      return *items_.emplace_back(std::make_unique<queue_entry>(self_)).get();
    }

    http_session& self_;
    std::vector<std::unique_ptr<queue_entry>> items_;
  };

  queue queue_;
  bool write_active_{false};

  boost::beast::flat_buffer buffer_;

  // The parser is stored in an optional container so we can
  // construct it from scratch it at the beginning of each new message.
  boost::optional<
      boost::beast::http::request_parser<boost::beast::http::string_body>>
      parser_;

  web_server::http_req_cb_t& http_req_cb_;
  web_server::ws_msg_cb_t& ws_msg_cb_;
  web_server::ws_open_cb_t& ws_open_cb_;
  web_server::ws_close_cb_t& ws_close_cb_;

  // Construct the session
  http_session(boost::beast::flat_buffer buffer,
               web_server::http_req_cb_t& http_req_cb,
               web_server::ws_msg_cb_t& ws_msg_cb,
               web_server::ws_open_cb_t& ws_open_cb,
               web_server::ws_close_cb_t& ws_close_cb)
      : queue_(*this),
        buffer_(std::move(buffer)),
        http_req_cb_(http_req_cb),
        ws_msg_cb_(ws_msg_cb),
        ws_open_cb_(ws_open_cb),
        ws_close_cb_(ws_close_cb) {}

  void do_read() {
    // Construct a new parser for each message
    parser_.emplace();

    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
    parser_->body_limit(10000);

    // Set the timeout.
    boost::beast::get_lowest_layer(derived().stream())
        .expires_after(std::chrono::seconds(30));

    // Read a request using the parser-oriented interface
    boost::beast::http::async_read(
        derived().stream(), buffer_, *parser_,
        boost::beast::bind_front_handler(&http_session::on_read,
                                         derived().shared_from_this()));
  }

  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if (ec == boost::beast::http::error::end_of_stream)
      return derived().do_eof();

    if (ec) {
      return fail(ec, "read");
    }

    // See if it is a WebSocket Upgrade
    if (boost::beast::websocket::is_upgrade(parser_->get())) {
      // Disable the timeout.
      // The websocket::stream uses its own timeout settings.
      boost::beast::get_lowest_layer(derived().stream()).expires_never();

      // Create a websocket session, transferring ownership
      // of both the socket and the HTTP request.
      return make_websocket_session(derived().release_stream(),
                                    parser_->release(), ws_msg_cb_, ws_open_cb_,
                                    ws_close_cb_);
    }

    auto& queue_entry = queue_.add_entry();
    if (http_req_cb_) {
      http_req_cb_(
          parser_->release(),
          [self = derived().shared_from_this(),
           &queue_entry](web_server::http_res_t&& res) {
            std::visit(queue_entry, std::move(res));
          },
          derived().is_ssl());
    } else {
      queue_entry(not_found(parser_->release()));
    }

    // If we aren't at the queue limit, try to pipeline another request
    if (!queue_.is_full()) do_read();
  }

  void on_write(bool close, boost::beast::error_code ec,
                std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    write_active_ = false;
    if (ec) {
      return fail(ec, "write");
    }

    if (close) {
      // This means we should close the connection, usually because
      // the response indicated the "Connection: close" semantic.
      return derived().do_eof();
    }

    // Inform the queue that a write completed
    if (queue_.on_write()) {
      // Read another request
      do_read();
    }
  }

  void send_next_response() {
    if (write_active_) {
      return;
    }
    queue_.send_next();
  }
};

//------------------------------------------------------------------------------

// Handles a plain HTTP connection
struct plain_http_session
    : public http_session<plain_http_session>,
      public std::enable_shared_from_this<plain_http_session> {
  boost::beast::tcp_stream stream_;

  // Create the session
  plain_http_session(boost::beast::tcp_stream&& stream,
                     boost::beast::flat_buffer&& buffer,
                     web_server::http_req_cb_t& http_req_cb,
                     web_server::ws_msg_cb_t& ws_msg_cb,
                     web_server::ws_open_cb_t& ws_open_cb,
                     web_server::ws_close_cb_t& ws_close_cb)
      : http_session<plain_http_session>(std::move(buffer), http_req_cb,
                                         ws_msg_cb, ws_open_cb, ws_close_cb),
        stream_(std::move(stream)) {}

  // Start the session
  void run() { this->do_read(); }

  // Called by the base class
  boost::beast::tcp_stream& stream() { return stream_; }

  // Called by the base class
  boost::beast::tcp_stream release_stream() { return std::move(stream_); }

  // Called by the base class
  void do_eof() {
    // Send a TCP shutdown
    boost::beast::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);

    // At this point the connection is closed gracefully
  }

  bool is_ssl() const { return false; }
};

//------------------------------------------------------------------------------

// Handles an SSL HTTP connection
struct ssl_http_session
    : public http_session<ssl_http_session>,
      public std::enable_shared_from_this<ssl_http_session> {
  boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;

  // Create the http_session
  ssl_http_session(boost::beast::tcp_stream&& stream,
                   boost::asio::ssl::context& ctx,
                   boost::beast::flat_buffer&& buffer,
                   web_server::http_req_cb_t& http_req_cb,
                   web_server::ws_msg_cb_t& ws_msg_cb,
                   web_server::ws_open_cb_t& ws_open_cb,
                   web_server::ws_close_cb_t& ws_close_cb)
      : http_session<ssl_http_session>(std::move(buffer), http_req_cb,
                                       ws_msg_cb, ws_open_cb, ws_close_cb),
        stream_(std::move(stream), ctx) {}

  // Start the session
  void run() {
    // Set the timeout.
    boost::beast::get_lowest_layer(stream_).expires_after(
        std::chrono::seconds(30));

    // Perform the SSL handshake
    // Note, this is the buffered version of the handshake.
    stream_.async_handshake(
        boost::asio::ssl::stream_base::server, buffer_.data(),
        boost::beast::bind_front_handler(&ssl_http_session::on_handshake,
                                         shared_from_this()));
  }

  // Called by the base class
  boost::beast::ssl_stream<boost::beast::tcp_stream>& stream() {
    return stream_;
  }

  // Called by the base class
  boost::beast::ssl_stream<boost::beast::tcp_stream> release_stream() {
    return std::move(stream_);
  }

  // Called by the base class
  void do_eof() {
    // Set the timeout.
    boost::beast::get_lowest_layer(stream_).expires_after(
        std::chrono::seconds(30));

    // Perform the SSL shutdown
    stream_.async_shutdown(boost::beast::bind_front_handler(
        &ssl_http_session::on_shutdown, shared_from_this()));
  }

  bool is_ssl() const { return true; }

private:
  void on_handshake(boost::beast::error_code ec, std::size_t bytes_used) {
    if (ec) {
      return fail(ec, "handshake");
    }

    // Consume the portion of the buffer used by the handshake
    buffer_.consume(bytes_used);

    do_read();
  }

  void on_shutdown(boost::beast::error_code ec) {
    if (ec) {
      return fail(ec, "shutdown");
    }

    // At this point the connection is closed gracefully
  }
};

//------------------------------------------------------------------------------

// Detects SSL handshakes
class detect_session : public std::enable_shared_from_this<detect_session> {
  boost::beast::tcp_stream stream_;
  boost::asio::ssl::context& ctx_;
  std::shared_ptr<std::string const> doc_root_;
  boost::beast::flat_buffer buffer_;

  web_server::http_req_cb_t& http_req_cb_;
  web_server::ws_msg_cb_t& ws_msg_cb_;
  web_server::ws_open_cb_t& ws_open_cb_;
  web_server::ws_close_cb_t& ws_close_cb_;

public:
  explicit detect_session(boost::asio::ip::tcp::socket&& socket,
                          boost::asio::ssl::context& ctx,
                          web_server::http_req_cb_t& http_req_cb,
                          web_server::ws_msg_cb_t& ws_msg_cb,
                          web_server::ws_open_cb_t& ws_open_cb,
                          web_server::ws_close_cb_t& ws_close_cb)
      : stream_(std::move(socket)),
        ctx_(ctx),
        http_req_cb_(http_req_cb),
        ws_msg_cb_(ws_msg_cb),
        ws_open_cb_(ws_open_cb),
        ws_close_cb_(ws_close_cb) {}

  // Launch the detector
  void run() {
    // Set the timeout.
    stream_.expires_after(std::chrono::seconds(30));

    boost::beast::async_detect_ssl(
        stream_, buffer_,
        boost::beast::bind_front_handler(&detect_session::on_detect,
                                         this->shared_from_this()));
  }

  void on_detect(boost::beast::error_code ec, bool result) {
    if (ec) return fail(ec, "detect");

    if (result) {
      // Launch SSL session
      std::make_shared<ssl_http_session>(std::move(stream_), ctx_,
                                         std::move(buffer_), http_req_cb_,
                                         ws_msg_cb_, ws_open_cb_, ws_close_cb_)
          ->run();
    } else {
      // Launch plain session
      std::make_shared<plain_http_session>(
          std::move(stream_), std::move(buffer_), http_req_cb_, ws_msg_cb_,
          ws_open_cb_, ws_close_cb_)
          ->run();
    }
  }
};

}  // namespace net
