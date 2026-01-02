#include "net/lb.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#if defined(__linux__)
#include <unistd.h>
#endif

#include "boost/asio/co_spawn.hpp"
#include "boost/asio/connect.hpp"
#include "boost/asio/detached.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/redirect_error.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/asio/steady_timer.hpp"
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
#include "utl/parser/arg_parser.h"
#include "utl/verify.h"
#include "utl/visit.h"

#include "net/base64.h"

using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;
namespace json = boost::json;
namespace ssl = boost::asio::ssl;
using namespace boost::asio;

namespace net {
using load_avg_t = unsigned;

using queue_entry_t = std::variant<load_avg_t, web_server::http_res_t>;

std::optional<web_server::http_req_t> parse_request(
    std::string_view raw, boost::beast::error_code& ec) {
  auto parser = http::request_parser<http::string_body>{};
  parser.eager(true);

  auto const n = parser.put(boost::asio::buffer(raw.data(), raw.size()), ec);

  if (ec) {
    return std::nullopt;
  }

  if (!parser.is_done()) {
    ec = http::error::need_more;
    return std::nullopt;
  }

  return parser.release();
}

template <bool isRequest, class Body, class Fields>
std::string to_str(http::message<isRequest, Body, Fields>& msg) {
  msg.prepare_payload();

  auto ec = boost::beast::error_code{};
  auto out = std::string{};
  auto serializer = http::serializer<isRequest, Body, Fields>{msg};
  while (!serializer.is_done()) {
    serializer.next(
        ec, [&](boost::beast::error_code& ec2, auto const& buffers) {
          ec2 = {};
          for (auto it = boost::asio::buffer_sequence_begin(buffers);
               it != boost::asio::buffer_sequence_end(buffers); ++it) {
            auto b = *it;
            out.append(static_cast<char const*>(b.data()), b.size());
          }
          serializer.consume(boost::asio::buffer_size(buffers));
        });

    if (ec) {
      throw boost::system::system_error(ec);
    }
  }

  return out;
}

std::string to_str(web_server::http_res_t& x) {
  return std::visit([](auto& x) { return to_str(x); }, x);
}

std::string to_str(load_avg_t const x) { return fmt::to_string(x); }

using wss_stream = websocket::stream<ssl::stream<tcp::socket>>;
using ws_stream = websocket::stream<tcp::socket>;

template <typename Stream>
struct conn : public lb::impl {
  conn(io_context& ioc, std::string const& url, web_server::http_req_cb_t cb)
      : ioc_{ioc},
        url_{url},
        http_callback_{std::move(cb)},
        ssl_ctx_{ssl::context::tls_client} {}

  ~conn() override { stop(); }

  void run() override {
    co_spawn(ioc_, loop(), detached);
    co_spawn(ioc_, loadavg_timer_loop(), detached);
  }

  void stop() override {
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
      } catch (std::exception const& e) {
        std::cout << "web socket lb error: " << e.what() << std::endl;
      }
      fmt::println("websocket {} disconnected, reconnecting in 1s", url_);
      std::this_thread::sleep_for(std::chrono::seconds{1});
      ws_.reset();
      write_in_progress_ = false;
      write_queue_ = {};
    }
  }

  awaitable<void> websocket_client_session() {
    auto executor = co_await this_coro::executor;

    constexpr auto const kDefaultPort =
        std::is_same_v<Stream, wss_stream> ? "443" : "80";

    auto url = boost::urls::url_view{url_};
    auto host = std::string{url.host()};
    auto port = url.has_port() ? std::string{url.port()} : kDefaultPort;
    auto path = url.path().empty() ? "/"
                                   : std::string{url.path()} + '?' +
                                         (url.has_query() ? url.query() : "");

    try {
      auto resolver = tcp::resolver{executor};

      if constexpr (std::is_same_v<Stream, wss_stream>) {
        ws_ = std::make_unique<Stream>(executor, ssl_ctx_);

        // Set SNI hostname (required for SSL)
        if (!SSL_set_tlsext_host_name(ws_->next_layer().native_handle(),
                                      host.c_str())) {
          auto const ec =
              boost::system::error_code{static_cast<int>(::ERR_get_error()),
                                        boost::asio::error::get_ssl_category()};
          throw boost::system::system_error{ec};
        }
      } else {
        ws_ = std::make_unique<Stream>(executor);
      }

      auto const results =
          co_await resolver.async_resolve(host, port, use_awaitable);

      // Connect to the server (TCP layer)
      auto ep = co_await async_connect(boost::beast::get_lowest_layer(*ws_),
                                       results, use_awaitable);

      if constexpr (std::is_same_v<Stream, wss_stream>) {
        co_await ws_->next_layer().async_handshake(ssl::stream_base::client,
                                                   use_awaitable);
      }

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

      std::cout << "LB " << host_port << ", awaiting requests." << std::endl;
      while (true) {
        auto buffer = boost::beast::multi_buffer{};
        co_await ws_->async_read(buffer, use_awaitable);
        auto message = boost::beast::buffers_to_string(buffer.data());
        co_spawn(executor, process_http_request(std::move(message)), detached);
      }
    } catch (std::exception const& e) {
      fmt::println(
          "LB session error, url={:?}, host={:?}, port={:?}, path={:?}: {}",
          url_, host, port, path, e.what());
    }
  }

  awaitable<void> loadavg_timer_loop() {
#if defined(__linux__)
    auto executor = co_await this_coro::executor;
    auto timer = steady_timer{executor};
    while (true) {
      double loads[3];
      if (getloadavg(loads, 1) == 1) {
        auto const cores = std::thread::hardware_concurrency();
        auto const pct = (loads[0] / static_cast<double>(cores)) * 100;
        queue_write(static_cast<load_avg_t>(std::lround(pct)));
      } else {
        std::cerr << "UNABLE TO GET loadavg\n";
      }

      timer.expires_after(std::chrono::seconds{5});
      auto ec = boost::system::error_code{};
      co_await timer.async_wait(redirect_error(use_awaitable, ec));
      if (ec == error::operation_aborted) {
        co_return;
      }
    }
#else
    std::cerr << "WARNING: LOAD AVG TIMER ONLY IMPLEMENTED FOR LINUX\n";
    co_return;
#endif
  }

  awaitable<void> process_write_queue() {
    if (write_in_progress_ || write_queue_.empty() || !ws_) {
      co_return;
    }

    write_in_progress_ = true;

    while (!write_queue_.empty() && ws_) {
      auto& msg = write_queue_.front();
      auto const message = std::visit([](auto& x) { return to_str(x); }, msg);
      ws_->binary(std::holds_alternative<web_server::http_res_t>(msg));
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

  void queue_write(queue_entry_t message) {
    write_queue_.push(std::move(message));
    if (!write_in_progress_) {
      co_spawn(ioc_, process_write_queue(), detached);
    }
  }

  awaitable<void> process_http_request(std::string ws_msg) {
    try {
      auto ec = boost::system::error_code{};
      auto req = parse_request(ws_msg, ec);
      utl::verify(req.has_value(), "Unable to parse request: {:?} [size={}]",
                  ws_msg.substr(0U, std::min(ws_msg.size(), std::size_t{200U})),
                  ws_msg.size());

      auto const id_str = req->at("x-request-id");
      auto const id =
          utl::parse_verify<unsigned>({id_str.data(), id_str.size()});

      // Call the HTTP request handler
      http_callback_(
          std::move(*req),
          [this, id](web_server::http_res_t&& response) {
            std::visit(
                [&](auto& res) { res.set("x-request-id", fmt::to_string(id)); },
                response);
            try {
              queue_write(std::move(response));
            } catch (std::exception const& e) {
              std::cerr << "Error preparing response for ID " << id << ": "
                        << e.what() << std::endl;
            }
          },
          false);
    } catch (std::out_of_range) {
      std::cerr << "Request without x-request-id header" << std::endl;
    } catch (std::exception const& e) {
      std::cerr << "Error processing HTTP request: " << e.what() << std::endl;
    }
    co_return;
  }

  io_context& ioc_;
  strand<io_context::executor_type> write_strand_{make_strand(ioc_)};
  std::queue<queue_entry_t> write_queue_;
  bool write_in_progress_{false};
  std::string url_;
  web_server::http_req_cb_t http_callback_;
  ssl::context ssl_ctx_;
  std::unique_ptr<Stream> ws_;
};

lb::impl::~impl() = default;

lb::lb(io_context& ios, std::string const& url, web_server::http_req_cb_t cb)
    : impl_(url.starts_with("wss://")
                ? static_cast<impl*>(new conn<wss_stream>{ios, url, cb})
                : static_cast<impl*>(new conn<ws_stream>{ios, url, cb})) {
  utl::verify(url.starts_with("wss://") || url.starts_with("ws://"),
              "invalid lbs url: {:?}", url);
}

lb::~lb() = default;

lb::lb(lb&&) = default;

lb& lb::operator=(lb&&) = default;

void lb::run() const { impl_->run(); }

void lb::stop() const { impl_->stop(); }

}  // namespace net
