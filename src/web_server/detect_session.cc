#include "net/web_server/detect_session.h"

#include <memory>

#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/ssl.hpp"

#include "net/web_server/fail.h"
#include "net/web_server/http_session.h"
#include "net/web_server/web_server.h"

namespace net {

// Detects SSL handshakes
struct detect_session : public std::enable_shared_from_this<detect_session> {
  explicit detect_session(boost::asio::ip::tcp::socket&& socket,
                          boost::asio::ssl::context& ctx,
                          web_server::http_req_cb_t& http_req_cb,
                          web_server::ws_msg_cb_t& ws_msg_cb,
                          web_server::ws_open_cb_t& ws_open_cb,
                          web_server::ws_close_cb_t& ws_close_cb,
                          std::chrono::nanoseconds const& timeout)
      : stream_(std::move(socket)),
        ctx_(ctx),
        http_req_cb_(http_req_cb),
        ws_msg_cb_(ws_msg_cb),
        ws_open_cb_(ws_open_cb),
        ws_close_cb_(ws_close_cb),
        timeout_(timeout) {}

  // Launch the detector
  void run() {
    // Set the timeout.
    stream_.expires_after(timeout_);

    boost::beast::async_detect_ssl(
        stream_, buffer_,
        boost::beast::bind_front_handler(&detect_session::on_detect,
                                         this->shared_from_this()));
  }

  void on_detect(boost::beast::error_code ec, bool result) {
    if (ec) {
      return fail(ec, "detect");
    }

    if (result) {
      // Launch SSL session
      make_http_session(std::move(stream_), ctx_, std::move(buffer_),
                        http_req_cb_, ws_msg_cb_, ws_open_cb_, ws_close_cb_,
                        timeout_);
    } else {
      // Launch plain session
      make_http_session(std::move(stream_), std::move(buffer_), http_req_cb_,
                        ws_msg_cb_, ws_open_cb_, ws_close_cb_, timeout_);
    }
  }

private:
  boost::beast::tcp_stream stream_;
  boost::asio::ssl::context& ctx_;
  boost::beast::flat_buffer buffer_;

  web_server::http_req_cb_t& http_req_cb_;
  web_server::ws_msg_cb_t& ws_msg_cb_;
  web_server::ws_open_cb_t& ws_open_cb_;
  web_server::ws_close_cb_t& ws_close_cb_;

  std::chrono::nanoseconds const& timeout_;
};

void make_detect_session(boost::asio::ip::tcp::socket&& socket,
                         boost::asio::ssl::context& ctx,
                         web_server::http_req_cb_t& http_req_cb,
                         web_server::ws_msg_cb_t& ws_msg_cb,
                         web_server::ws_open_cb_t& ws_open_cb,
                         web_server::ws_close_cb_t& ws_close_cb,
                         std::chrono::nanoseconds const& timeout) {
  std::make_shared<detect_session>(std::move(socket), ctx, http_req_cb,
                                   ws_msg_cb, ws_open_cb, ws_close_cb, timeout)
      ->run();
}

}  // namespace net
