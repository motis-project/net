#include "net/web_server/detect_session.h"

#include <iostream>

#include "boost/asio/post.hpp"
#include "boost/beast/core/detect_ssl.hpp"
#include "boost/beast/version.hpp"

#include "net/web_server/http_session.h"

namespace asio = boost::asio;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;

namespace net {

inline void fail(boost::system::error_code ec, char const* what) {
  std::cerr << what << ": " << ec.message() << "\n";
}

detect_session::detect_session(session_manager& session_mgr, tcp::socket socket,
                               ssl::context& ctx,
                               web_server::http_req_cb_t& http_req_cb,
                               web_server::ws_msg_cb_t& ws_msg_cb,
                               web_server::ws_open_cb_t& ws_open_cb,
                               web_server::ws_close_cb_t& ws_close_cb)
    : session_mgr_{session_mgr},
      stream_{std::move(socket)},
      ctx_{ctx},
      http_req_cb_{http_req_cb},
      ws_msg_cb_{ws_msg_cb},
      ws_open_cb_{ws_open_cb},
      ws_close_cb_{ws_close_cb} {
  session_mgr_.add(this);
}

detect_session::~detect_session() { session_mgr_.remove(this); }

void detect_session::run() {
  std::cout << "detect_session::run()" << std::endl;
  // stream_.expires_after(std::chrono::seconds(30));
  boost::beast::async_detect_ssl(
      stream_, buffer_,
      boost::beast::bind_front_handler(&detect_session::on_detect,
                                       this->shared_from_this()));
}

void detect_session::stop() { boost::beast::get_lowest_layer(stream_).close(); }

void detect_session::on_detect(boost::beast::error_code ec, bool result) {
  std::cout << "detect_session::on_detect(): result=" << result << std::endl;
  if (ec) {
    return fail(ec, "detect");
  }

  if (result) {
    std::make_shared<http_session>(session_mgr_, std::move(stream_),
                                   std::move(buffer_), ctx_, http_req_cb_,
                                   ws_msg_cb_, ws_open_cb_, ws_close_cb_)
        ->run();
  } else {
    std::make_shared<http_session>(session_mgr_, std::move(stream_),
                                   std::move(buffer_), ctx_, http_req_cb_,
                                   ws_msg_cb_, ws_open_cb_, ws_close_cb_)
        ->run();
  }
}

}  // namespace net