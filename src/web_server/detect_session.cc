#include "net/web_server/detect_session.h"

#include <memory>

#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"

#if defined(NET_TLS)
#include "boost/beast/ssl.hpp"
#endif

#include "net/web_server/fail.h"
#include "net/web_server/http_session.h"
#include "net/web_server/web_server.h"

namespace net {

#if defined(NET_TLS)
// Detects SSL handshakes
struct detect_session : public std::enable_shared_from_this<detect_session> {
  explicit detect_session(boost::asio::ip::tcp::socket&& socket,
                          boost::asio::ssl::context& ctx,
                          web_server_settings_ptr settings)
      : stream_(std::move(socket)), ctx_(ctx), settings_(std::move(settings)) {}

  // Launch the detector
  void run() {
    // Set the timeout.
    stream_.expires_after(settings_->timeout_);

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
                        std::move(settings_));
    } else {
      // Launch plain session
      make_http_session(std::move(stream_), std::move(buffer_),
                        std::move(settings_));
    }
  }

private:
  boost::beast::tcp_stream stream_;
  boost::asio::ssl::context& ctx_;
  boost::beast::flat_buffer buffer_;

  web_server_settings_ptr settings_;
};

void make_detect_session(boost::asio::ip::tcp::socket&& socket,
                         boost::asio::ssl::context& ctx,
                         web_server_settings_ptr const& settings) {
  std::make_shared<detect_session>(std::move(socket), ctx, settings)->run();
}
#else
void make_detect_session(boost::asio::ip::tcp::socket&& socket,
                         web_server_settings_ptr const& settings) {
  make_http_session(boost::beast::tcp_stream{std::move(socket)},
                    boost::beast::flat_buffer{}, settings);
}
#endif

}  // namespace net
