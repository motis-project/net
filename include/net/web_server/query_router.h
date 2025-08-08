#pragma once

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "utl/helpers/algorithm.h"
#include "utl/overloaded.h"
#include "utl/verify.h"

#include "boost/fiber/buffered_channel.hpp"
#include "boost/json.hpp"
#include "boost/url.hpp"

#include "openapi/missing_param_exception.h"

#include "net/base64.h"
#include "net/web_server/content_encoding.h"
#include "net/web_server/enable_cors.h"
#include "net/web_server/responses.h"
#include "net/web_server/serve_static.h"
#include "net/web_server/url_decode.h"
#include "net/web_server/web_server.h"

namespace net {

using request = web_server::http_req_t;
using reply = web_server::http_res_t;

struct route_request : public request {
  route_request(request req) : web_server::http_req_t{std::move(req)} {}
  std::string username_, password_;
};

template <typename T>
concept JSON =
    boost::json::has_value_to<T>::value &&
    boost::json::has_value_from<T>::value &&
    !std::is_same_v<T, std::string>;  // avoid ambiguity with string handlers

template <typename Fn>
concept StringGetHandler = requires(boost::urls::url_view const& url, Fn f) {
  { f(url) } -> std::same_as<std::string>;
};

template <typename Fn>
concept StringPostHandler = requires(std::string_view const& req, Fn f) {
  { f(req) } -> std::same_as<std::string>;
};

template <typename Fn>
concept JsonPostHandler = requires(Fn f, typename utl::first_argument<Fn> arg) {
  { f(arg) } -> JSON;
};

template <typename Fn>
concept JsonGetHandler = requires(boost::urls::url_view const& url, Fn f) {
  { f(url) } -> JSON;
};

struct default_exec {
  void exec(auto&& fn, web_server::http_res_cb_t cb) { cb(fn()); }
};

struct asio_exec {
  asio_exec(boost::asio::io_context& io, boost::asio::io_context& worker_pool);

  void exec(auto&& f, web_server::http_res_cb_t cb) {
    worker_pool_.post([&, f = std::move(f), cb = std::move(cb)]() mutable {
      try {
        auto res = std::make_shared<web_server::http_res_t>(f());
        io_.post([cb = std::move(cb), res = std::move(res)]() mutable {
          cb(std::move(*res));
        });
      } catch (...) {
        std::cerr << "UNEXPECTED EXCEPTION\n";

        auto str = web_server::string_res_t{
            boost::beast::http::status::internal_server_error, 11};
        str.body() = "error";
        str.prepare_payload();

        auto res = std::make_shared<web_server::http_res_t>(str);
        io_.post([cb = std::move(cb), res = std::move(res)]() mutable {
          cb(std::move(*res));
        });
      }
    });
  }

  boost::asio::io_context& io_;
  boost::asio::io_context& worker_pool_;
};

struct fiber_exec {
  using task_t = std::function<void()>;
  using channel_t = boost::fibers::buffered_channel<task_t>;

  fiber_exec(boost::asio::io_context& io, channel_t& ch) : io_{io}, ch_{ch} {}

  void exec(auto&& f, net::web_server::http_res_cb_t cb) {
    auto const result =
        ch_.try_push([&, f = std::move(f), cb = std::move(cb)]() {
          auto res = std::make_shared<net::web_server::http_res_t>(f());
          io_.post([cb = std::move(cb), res = std::move(res)]() mutable {
            cb(std::move(*res));
          });
        });
    if (result == boost::fibers::channel_op_status::full) {
      auto str = web_server::string_res_t{
          boost::beast::http::status::too_many_requests, 11};
      str.prepare_payload();
      auto res = std::make_shared<web_server::http_res_t>(str);
      io_.post([cb = std::move(cb), res = std::move(res)]() mutable {
        cb(std::move(*res));
      });
    } else if (result != boost::fibers::channel_op_status::success) {
      auto str = web_server::string_res_t{
          boost::beast::http::status::internal_server_error, 11};
      str.body() = fmt::format(
          R"({{"error": "channel status {}"}})",
          static_cast<std::underlying_type_t<boost::fibers::channel_op_status>>(
              result));
      str.prepare_payload();
      auto res = std::make_shared<web_server::http_res_t>(str);
      io_.post([cb = std::move(cb), res = std::move(res)]() mutable {
        cb(std::move(*res));
      });
    }
  }

  boost::asio::io_context& io_;
  channel_t& ch_;
};

using route_request_handler = std::function<reply(route_request, bool)>;

struct header {
  std::string key_;
  std::string value_;
};

struct handler {
  std::string method_;
  std::string prefix_;
  route_request_handler request_handler_;
};

template <typename Executor = default_exec>
struct query_router {
  explicit query_router(Executor&& exec) : exec_{std::move(exec)} {}

  query_router& route(std::string method, std::string prefix,
                      route_request_handler h) {
    routes_.push_back({std::move(method), std::move(prefix), std::move(h)});
    return *this;
  }

  template <StringPostHandler Fn>
  query_router& route(std::string method, std::string const& path_regex,
                      Fn&& fn) {
    return route(std::move(method), path_regex,
                 [fn = std::forward<Fn>(fn)](web_server::http_req_t const& req,
                                             bool is_ssl) -> reply {
                   auto res = net::web_server::string_res_t{
                       boost::beast::http::status::ok, req.version()};
                   set_response_body(res, req, fn(req.body()));
                   res.keep_alive(req.keep_alive());
                   return res;
                 });
  }

  template <StringGetHandler Fn>
  query_router& get(std::string const& path_regex, Fn&& fn) {
    namespace http = boost::beast::http;
    namespace json = boost::json;
    return route(
        "GET", path_regex,
        [fn = std::forward<Fn>(fn)](route_request const& req,
                                    bool const ssl) -> reply {
          auto res = web_server::string_res_t{http::status::ok, req.version()};
          set_response_body(res, req, fn(boost::url_view{req.target()}));
          res.keep_alive(req.keep_alive());
          return res;
        });
  }

  template <JsonPostHandler Fn>
  query_router& post(std::string const& path_regex, Fn&& fn) {
    return route(
        "POST", path_regex,
        [fn = std::forward<Fn>(fn)](web_server::http_req_t const& req,
                                    bool is_ssl) -> reply {
          auto res = net::web_server::string_res_t{
              boost::beast::http::status::ok, req.version()};
          res.set(boost::beast::http::field::content_type, "application/json");
          set_response_body(
              res, req,
              boost::json::serialize(boost::json::value_from(fn(
                  boost::json::value_to<std::decay_t<utl::first_argument<Fn>>>(
                      boost::json::parse(req.body()))))));
          res.keep_alive(req.keep_alive());
          return res;
        });
  }

  template <JsonGetHandler Fn>
  query_router& get(std::string const& path_regex, Fn&& fn) {
    namespace http = boost::beast::http;
    namespace json = boost::json;
    return route("GET", path_regex,
                 [fn = std::forward<Fn>(fn)](route_request const& req,
                                             bool const ssl) -> reply {
                   auto res = web_server::string_res_t{http::status::ok,
                                                       req.version()};
                   res.set(http::field::content_type, "application/json");
                   set_response_body(res, req,
                                     json::serialize(json::value_from(
                                         fn(boost::url_view{req.target()}))));
                   res.keep_alive(req.keep_alive());
                   return res;
                 });
  }

  void operator()(web_server::http_req_t req, web_server::http_res_cb_t cb,
                  bool is_ssl) {
    auto const url = boost::urls::url_view{req.target()};
    auto const path = url.path();

    auto route = utl::find_if(routes_, [&](handler const& h) {
      return (h.method_ == "*" || h.method_ == req.method_string()) &&
             path.starts_with(h.prefix_);
    });

    if (route == end(routes_)) {
      auto rep = reply{not_found_response(req)};
      if (reply_hook_) {
        reply_hook_(rep);
      }
      return cb(std::move(rep));
    }

    auto route_req = route_request{std::move(req)};

    set_credentials(route_req);
    decode_content(route_req);

    return exec_.exec(
        [this, route, is_ssl, r = std::move(route_req)]() {
          reply rep;
          try {
            rep = route->request_handler_(r, is_ssl);
          } catch (openapi::missing_param_exception const& e) {
            using namespace boost::json;
            rep = bad_request_response(
                r,
                serialize(value{{"error", fmt::format("missing parameter: {}",
                                                      e.param_)}}));
          } catch (std::exception const& e) {
            using namespace boost::json;
            rep =
                server_error_response(r, serialize(value{{"error", e.what()}}));
          } catch (...) {
            rep = reply{server_error_response(r)};
          }
          if (reply_hook_) {
            try {
              reply_hook_(rep);
            } catch (...) {
              std::cerr << "query_router: unhandled exception in reply hook\n";
            }
          }
          // Add headers
          std::visit(
              [&](auto& r) {
                for (auto const& header : headers_) {
                  r.set(header.key_, header.value_);
                }
              },
              rep);
          return std::move(rep);
        },
        std::move(cb));
  }

  void reply_hook(std::function<void(reply&)> reply_hook) {
    reply_hook_ = std::move(reply_hook);
  }

  void enable_cors() {
    reply_hook([](reply& rep) { net::enable_cors(rep); });
    route("OPTIONS", "",
          [](route_request const& req, bool) { return empty_response(req); });
  }

  void add_header(std::string key, std::string value) {
    headers_.push_back(header{std::move(key), std::move(value)});
  }

  void serve_files(std::filesystem::path const& p) {
    route("GET", "",
          [p](route_request const& req, bool) -> web_server::http_res_t {
            if (auto res = serve_static_file(p, req); res.has_value()) {
              return std::move(*res);
            } else {
              namespace http = boost::beast::http;
              return net::web_server::string_res_t{http::status::not_found,
                                                   req.version()};
            }
          });
  }

private:
  void decode_content(request& req) {
    if (auto const it =
            req.base().find(boost::beast::http::field::content_type);
        it != end(req.base()) &&
        it->value().find("urlencoded") != std::string_view::npos) {
      std::string decoded_content;
      url_decode(req.body(), decoded_content);
      req.body() = decoded_content;
    }
  }

  void set_credentials(route_request& req) {
    if (auto const it =
            req.base().find(boost::beast::http::field::authorization);
        it != req.base().end() && it->value().length() > 6) {
      auto const auth = it->value().substr(6);
      auto const credentials =
          decode_base64(std::string{auth.data(), auth.size()});

      auto const split = credentials.find_first_of(':');
      if (split == std::string::npos) {
        return;
      }

      req.username_ = credentials.substr(0, split);
      req.password_ = credentials.substr(split + 1, std::string::npos);
    }
  }

  std::vector<header> headers_;
  std::vector<handler> routes_;
  std::function<void(reply&)> reply_hook_;

  Executor exec_;
};

}  // namespace net
