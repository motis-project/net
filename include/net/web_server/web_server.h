#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <variant>

#include "boost/asio/io_context.hpp"
#include "boost/asio/ssl/context.hpp"
#include "boost/beast/http/buffer_body.hpp"
#include "boost/beast/http/empty_body.hpp"
#include "boost/beast/http/file_body.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/string_body.hpp"

namespace net {

enum class ws_msg_type { TEXT, BINARY };

struct ws_session {
  using send_cb_t = std::function<void(boost::system::error_code, size_t)>;
  virtual void send(std::string msg, ws_msg_type type, send_cb_t cb) = 0;
};

using ws_session_ptr = std::weak_ptr<ws_session>;

struct web_server {
  using http_req_t =
      boost::beast::http::request<boost::beast::http::string_body>;
  using string_res_t =
      boost::beast::http::response<boost::beast::http::string_body>;
  using buffer_res_t =
      boost::beast::http::response<boost::beast::http::buffer_body>;
  using file_res_t =
      boost::beast::http::response<boost::beast::http::file_body>;
  using empty_res_t =
      boost::beast::http::response<boost::beast::http::empty_body>;
  using http_res_t =
      std::variant<string_res_t, buffer_res_t, file_res_t, empty_res_t>;

  using http_res_cb_t = std::function<void(http_res_t&&)>;
  using http_req_cb_t = std::function<void(http_req_t, http_res_cb_t, bool)>;

  using ws_msg_cb_t =
      std::function<void(ws_session_ptr, std::string const&, ws_msg_type)>;
  using ws_open_cb_t = std::function<void(ws_session_ptr, bool)>;
  using ws_close_cb_t = std::function<void(void*)>;

  explicit web_server(boost::asio::io_context&, boost::asio::ssl::context&);
  ~web_server();

  web_server(web_server&&) = default;
  web_server& operator=(web_server&&) = default;

  web_server(web_server const&) = delete;
  web_server& operator=(web_server const&) = delete;

  void init(std::string const& host, std::string const& port,
            boost::system::error_code& ec);
  void run();
  void stop();

  void set_timeout(std::chrono::nanoseconds const& timeout);
  void set_request_body_limit(std::uint64_t limit);
  void set_request_queue_limit(std::size_t limit);

  void on_http_request(http_req_cb_t);
  void on_ws_msg(ws_msg_cb_t);
  void on_ws_open(ws_open_cb_t);
  void on_ws_close(ws_close_cb_t);

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace net
