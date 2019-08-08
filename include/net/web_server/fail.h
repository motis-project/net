#pragma once

#include <iostream>

#include "boost/asio/ssl/error.hpp"
#include "boost/beast/core/error.hpp"

namespace net {

// Report a failure
inline void fail(boost::beast::error_code ec, char const* what) {
  // ssl::error::stream_truncated, also known as an SSL "short read",
  // indicates the peer closed the connection without performing the
  // required closing handshake (for example, Google does this to
  // improve performance). Generally this can be a security issue,
  // but if your communication protocol is self-terminated (as
  // it is with both HTTP and WebSocket) then you may simply
  // ignore the lack of close_notify.
  //
  // https://github.com/boostorg/beast/issues/38
  //
  // https://security.stackexchange.com/questions/91435/how-to-handle-a-malicious-ssl-tls-shutdown
  //
  // When a short read would cut off the end of an HTTP message,
  // Beast returns the error boost::beast::http::error::partial_message.
  // Therefore, if we see a short read here, it has occurred
  // after the message has been completed, so it is safe to ignore it.

  if (ec == boost::asio::ssl::error::stream_truncated ||
      ec == boost::beast::error::timeout) {
    return;
  }

  std::cerr << what << ": " << ec.message() << "\n";
}

}  // namespace net
