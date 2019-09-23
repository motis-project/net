#include "net/web_server/http_session.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "boost/beast/http.hpp"
#include "boost/beast/ssl.hpp"
#include "boost/beast/websocket/rfc6455.hpp"

#include "net/web_server/fail.h"
#include "net/web_server/responses.h"
#include "net/web_server/web_server.h"
#include "net/web_server/websocket_session.h"

namespace net {

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
    // The type-erased, saved work item
    struct response {
      response() = default;
      virtual ~response() = default;
      response(response const&) = delete;
      response& operator=(response const&) = delete;
      response(response&&) = delete;
      response& operator=(response&&) = delete;

      virtual void send() = 0;
    };

    explicit queue(http_session& self, std::size_t limit)
        : self_(self), limit_(limit) {
      items_.reserve(limit);
    }

    ~queue() = default;
    queue(queue const&) = delete;
    queue& operator=(queue const&) = delete;
    queue(queue&&) = delete;
    queue& operator=(queue&&) = delete;

    // Returns `true` if we have reached the queue limit
    bool is_full() const { return items_.size() >= limit_; }

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
      items_.front()->response_->send();
      return true;
    }

    struct pending_request {
      explicit pending_request(http_session& session) : self_(session) {}

      bool is_finished() const { return static_cast<bool>(response_); }

      // Called by the HTTP handler to send a response.
      template <bool IsRequest, class Body, class Fields>
      void operator()(
          boost::beast::http::message<IsRequest, Body, Fields>&& msg) {
        // This holds a work item
        struct response_impl : response {
          http_session& self_;
          boost::beast::http::message<IsRequest, Body, Fields> msg_;

          response_impl(
              http_session& self,
              boost::beast::http::message<IsRequest, Body, Fields>&& msg)
              : self_(self), msg_(std::move(msg)) {}

          void send() override {
            boost::beast::http::async_write(
                self_.derived().stream(), msg_,
                boost::beast::bind_front_handler(
                    &http_session::on_write, self_.derived().shared_from_this(),
                    msg_.need_eof()));
          }
        };

        response_ = std::make_unique<response_impl>(self_, std::move(msg));
        boost::asio::post(self_.derived().stream().get_executor(),
                          [&, self = self_.derived().shared_from_this()]() {
                            self_.send_next_response();
                          });
      }

      http_session& self_;
      std::unique_ptr<response> response_;
    };

    pending_request& add_entry() {
      return *items_.emplace_back(std::make_unique<pending_request>(self_))
                  .get();
    }

    http_session& self_;
    std::vector<std::unique_ptr<pending_request>> items_;
    // Maximum number of responses we will queue
    std::size_t limit_;
  };

  // Construct the session
  http_session(boost::beast::flat_buffer buffer,
               web_server_settings const& settings)
      : queue_(*this, settings.request_queue_limit_),
        buffer_(std::move(buffer)),
        settings_(settings) {}

  void do_read() {
    // Construct a new parser for each message
    parser_.emplace();

    // Apply a reasonable limit to the allowed size
    // of the body in bytes to prevent abuse.
    parser_->body_limit(settings_.request_body_limit_);

    // Set the timeout.
    boost::beast::get_lowest_layer(derived().stream())
        .expires_after(settings_.timeout_);

    // Read a request using the parser-oriented interface
    boost::beast::http::async_read(
        derived().stream(), buffer_, *parser_,
        boost::beast::bind_front_handler(&http_session::on_read,
                                         derived().shared_from_this()));
  }

  void on_read(boost::beast::error_code ec, std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);

    // This means they closed the connection
    if (ec == boost::beast::http::error::end_of_stream) {
      return derived().do_eof();
    }

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
                                    parser_->release(), settings_);
    }

    auto& queue_entry = queue_.add_entry();
    if (settings_.http_req_cb_) {
      settings_.http_req_cb_(
          parser_->release(),
          [self = derived().shared_from_this(),
           &queue_entry](web_server::http_res_t&& res) {
            std::visit(queue_entry, std::move(res));
          },
          derived().is_ssl());
    } else {
      queue_entry(
          not_found_response(parser_->release(), "No handler implemented"));
    }

    // If we aren't at the queue limit, try to pipeline another request
    if (!queue_.is_full()) {
      do_read();
    }
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

  queue queue_;
  bool write_active_{false};

  boost::beast::flat_buffer buffer_;

  // The parser is stored in an optional container so we can
  // construct it from scratch it at the beginning of each new message.
  std::optional<
      boost::beast::http::request_parser<boost::beast::http::string_body>>
      parser_;

  web_server_settings const& settings_;
};

//------------------------------------------------------------------------------

// Handles a plain HTTP connection
struct plain_http_session
    : public http_session<plain_http_session>,
      public std::enable_shared_from_this<plain_http_session> {
  // Create the session
  plain_http_session(boost::beast::tcp_stream&& stream,
                     boost::beast::flat_buffer&& buffer,
                     web_server_settings const& settings)
      : http_session<plain_http_session>(std::move(buffer), settings),
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

  boost::beast::tcp_stream stream_;
};

//------------------------------------------------------------------------------

void make_http_session(boost::beast::tcp_stream&& stream,
                       boost::beast::flat_buffer&& buffer,
                       web_server_settings const& settings) {
  std::make_shared<plain_http_session>(std::move(stream), std::move(buffer),
                                       settings)
      ->run();
}

//------------------------------------------------------------------------------

// Handles an SSL HTTP connection
struct ssl_http_session
    : public http_session<ssl_http_session>,
      public std::enable_shared_from_this<ssl_http_session> {
  // Create the http_session
  ssl_http_session(boost::beast::tcp_stream&& stream,
                   boost::asio::ssl::context& ctx,
                   boost::beast::flat_buffer&& buffer,
                   web_server_settings const& settings)
      : http_session<ssl_http_session>(std::move(buffer), settings),
        stream_(std::move(stream), ctx) {}

  // Start the session
  void run() {
    // Set the timeout.
    boost::beast::get_lowest_layer(stream_).expires_after(settings_.timeout_);

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
    boost::beast::get_lowest_layer(stream_).expires_after(settings_.timeout_);

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

  boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
};

//------------------------------------------------------------------------------

void make_http_session(boost::beast::tcp_stream&& stream,
                       boost::asio::ssl::context& ctx,
                       boost::beast::flat_buffer&& buffer,
                       web_server_settings const& settings) {
  std::make_shared<ssl_http_session>(std::move(stream), ctx, std::move(buffer),
                                     settings)
      ->run();
}

}  // namespace net
