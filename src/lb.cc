#include "net/lb.h"

#include <cstdint>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/connect.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/asio/strand.hpp"
#include "boost/asio/use_awaitable.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/beast/ssl.hpp"
#include "boost/beast/websocket.hpp"
#include "boost/beast/websocket/ssl.hpp"
#include "boost/json.hpp"
#include "boost/url.hpp"

#include "utl/overloaded.h"
#include "utl/verify.h"

#include "net/base64.h"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;
namespace json = boost::json;
namespace ssl = boost::asio::ssl;
using namespace boost::asio;

namespace net {

std::pair<web_server::http_req_t, std::int64_t> to_beast_request(
    json::value const& v) {
  auto ret = std::pair<web_server::http_req_t, std::int64_t>{};
  auto& [req, id] = ret;
  id = v.at("id").get_int64();
  req.version(11);
  req.method(http::string_to_verb(v.at("method").as_string()));
  req.target(v.at("uri").as_string());
  for (auto const& [key, value] : v.at("headers").as_object()) {
    req.set(key, value.as_string());
  }
  auto const& body = v.at("body");
  if (body.is_string()) {
    req.body() = decode_base64(std::string{body.as_string()});
  }
  req.prepare_payload();
  return ret;
}

json::value to_json_response(std::int64_t const id,
                             web_server::http_res_t&& http_res) {
  return std::visit(
      utl::overloaded{
          [&](web_server::string_res_t&& r) {
            auto obj = json::object{};
            obj["id"] = id;
            obj["status"] = static_cast<std::uint16_t>(r.result_int());

            auto headers_obj = json::object{};
            for (auto const& header : r) {
              if (header.name() != http::field::connection) {
                headers_obj[header.name_string()] = header.value();
              }
            }
            obj["headers"] = std::move(headers_obj);
            obj["body"] = encode_base64(r.body());

            return obj;
          },
          [&](web_server::file_res_t&& r) {
            auto const size = r.body().size();
            auto body = std::string(size, '\0');
            auto ec = boost::beast::error_code{};
            auto const bytes_read =
                r.body().file().read(body.data(), body.size(), ec);

            auto obj = json::object{};
            obj["id"] = id;
            obj["status"] = static_cast<std::uint16_t>(r.result_int());
            auto headers_obj = json::object{};
            for (auto const& header : r) {
              headers_obj[header.name_string()] = header.value();
            }
            obj["headers"] = std::move(headers_obj);
            obj["body"] = encode_base64(std::move(body));

            return obj;
          },
          [](web_server::empty_res_t&&) { return json::object{}; },
          [](web_server::buffer_res_t&&) { return json::object{}; }},
      std::move(http_res));
}

struct lb::impl {
  impl(io_context& ioc, std::string const& url, web_server::http_req_cb_t cb)
      : ioc_{ioc},
        url_{url},
        http_callback_{std::move(cb)},
        ssl_ctx_{ssl::context::tls_client} {}

  ~impl() { stop(); }

  void run() { co_spawn(ioc_, loop(), detached); }

  void stop() {
    if (ws_) {
      boost::system::error_code ec;
      ws_->close(websocket::close_code::normal, ec);
    }
  }

  awaitable<void> loop() {
    while (true) {
      try {
        ssl_ctx_ = ssl::context{ssl::context::tls_client};
        ssl_ctx_.set_default_verify_paths();
        ssl_ctx_.set_verify_mode(ssl::verify_peer);
        ssl_ctx_.set_verify_callback(
            [](bool preverified, ssl::verify_context& ctx) { return true; });

        co_await websocket_client_session();
      } catch (...) {
      }
      fmt::println("websocket {} disconnected, reconnecting in 1s", url_);
      std::this_thread::sleep_for(std::chrono::seconds{1});
      ws_.reset();
      write_in_progress_ = false;
      write_queue_ = std::queue<std::string>{};
    }
  }

  awaitable<void> websocket_client_session() {
    auto executor = co_await this_coro::executor;

    try {
      auto url = boost::urls::url_view(url_);
      auto host = std::string(url.host());
      auto port = url.has_port() ? std::string(url.port()) : "443";
      auto path = url.path().empty() ? "/"
                                     : std::string(url.path()) + '?' +
                                           (url.has_query() ? url.query() : "");

      auto resolver = tcp::resolver{executor};
      ws_ = std::make_unique<websocket::stream<ssl::stream<tcp::socket>>>(
          executor, ssl_ctx_);

      // Set SNI hostname (required for SSL)
      if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(),
                                    host.c_str())) {
        auto const ec =
            boost::system::error_code{static_cast<int>(::ERR_get_error()),
                                      boost::asio::error::get_ssl_category()};
        throw boost::system::system_error{ec};
      }

      auto const results =
          co_await resolver.async_resolve(host, port, use_awaitable);

      // Connect to the server (TCP layer)
      auto ep = co_await async_connect(boost::beast::get_lowest_layer(*ws_),
                                       results, use_awaitable);

      co_await ws_->next_layer().async_handshake(ssl::stream_base::client,
                                                 use_awaitable);

      // boost::beast::get_lowest_layer(*ws_).expires_never();  // TODO

      ws_->set_option(websocket::stream_base::timeout::suggested(
          boost::beast::role_type::client));
      ws_->set_option(
          websocket::stream_base::decorator([&](websocket::request_type& req) {
            req.set(http::field::user_agent, "MOTIS lb/1.0");
            req.set(http::field::host, host);
          }));

      // Websocket handshake (upgrade request).
      auto host_port = host + ':' + std::to_string(ep.port());
      co_await ws_->async_handshake(host_port, path, use_awaitable);

      while (true) {
        auto buffer = boost::beast::multi_buffer{};
        co_await ws_->async_read(buffer, use_awaitable);
        auto message = boost::beast::buffers_to_string(buffer.data());
        co_spawn(executor, process_http_request(std::move(message)), detached);
      }
    } catch (std::exception const& e) {
      std::cerr << "WebSocket session error: " << e.what() << std::endl;
    }
  }

  awaitable<void> process_write_queue() {
    if (write_in_progress_ || write_queue_.empty() || !ws_) {
      co_return;
    }

    write_in_progress_ = true;

    while (!write_queue_.empty() && ws_) {
      auto message = std::move(write_queue_.front());
      write_queue_.pop();

      try {
        auto const bytes_written =
            co_await ws_->async_write(buffer(message), use_awaitable);
      } catch (std::exception const& e) {
        std::cerr << "Error in write: " << e.what() << std::endl;
        break;
      }
    }

    write_in_progress_ = false;
  }

  void queue_write(std::string message) {
    write_queue_.push(std::move(message));
    if (!write_in_progress_) {
      co_spawn(ioc_, process_write_queue(), detached);
    }
  }

  awaitable<void> process_http_request(std::string ws_msg) {
    try {
      auto const [req, id] = to_beast_request(json::parse(ws_msg));

      // Call the HTTP request handler
      http_callback_(
          std::move(req),
          [this, id](web_server::http_res_t&& response) {
            try {
              queue_write(std::move(
                  json::serialize(to_json_response(id, std::move(response)))));
            } catch (std::exception const& e) {
              std::cerr << "Error preparing response for ID " << id << ": "
                        << e.what() << std::endl;
            }
          },
          false);
    } catch (std::exception const& e) {
      std::cerr << "Error processing HTTP request: " << e.what() << std::endl;
    }
    co_return;
  }

  io_context& ioc_;
  strand<io_context::executor_type> write_strand_{make_strand(ioc_)};
  std::queue<std::string> write_queue_;
  bool write_in_progress_{false};
  std::string url_;
  web_server::http_req_cb_t http_callback_;
  ssl::context ssl_ctx_;
  std::unique_ptr<websocket::stream<ssl::stream<tcp::socket>>> ws_;
};

// Implementation of the lb class methods
lb::lb(io_context& ios, std::string const& url, web_server::http_req_cb_t cb)
    : impl_(std::make_unique<impl>(ios, url, cb)) {}

lb::~lb() = default;

lb::lb(lb&&) = default;

lb& lb::operator=(lb&&) = default;

void lb::run() const { impl_->run(); }

void lb::stop() const { impl_->stop(); }

}  // namespace net