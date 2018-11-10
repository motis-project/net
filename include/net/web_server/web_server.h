#pragma once

#include <functional>
#include <memory>
#include <string>

#include "boost/asio/io_context.hpp"
#include "boost/asio/ssl/context.hpp"
#include "boost/beast/http/message.hpp"
#include "boost/beast/http/string_body.hpp"

namespace net {

struct ws_session;
struct session_manager;

using ws_session_ptr = std::weak_ptr<ws_session>;

enum class ws_msg_type { TEXT, BINARY };

void ws_send(std::shared_ptr<ws_session> const&, std::string msg, ws_msg_type,
             std::function<void(boost::system::error_code, size_t)> cb);

void ws_broadcast(session_manager const*, std::string msg, ws_msg_type,
                  std::function<void(boost::system::error_code, size_t)> cb);

struct web_server {
  using http_req_t =
      boost::beast::http::request<boost::beast::http::string_body>;
  using http_res_t =
      boost::beast::http::response<boost::beast::http::string_body>;
  using http_res_cb_t = std::function<void(http_res_t)>;
  using http_req_cb_t = std::function<void(http_req_t, http_res_cb_t)>;
  using ws_msg_cb_t =
      std::function<void(ws_session_ptr, std::string const&, ws_msg_type)>;
  using ws_open_cb_t = std::function<void(ws_session_ptr)>;
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

  void on_http_request(http_req_cb_t);
  void on_ws_msg(ws_msg_cb_t);
  void on_ws_open(ws_open_cb_t);
  void on_ws_close(ws_close_cb_t);

  session_manager const* get_session_mgr() const;

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace net
