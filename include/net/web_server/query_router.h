#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "utl/overloaded.h"

#include "boost/json.hpp"
#include "boost/url.hpp"

#include "net/web_server/web_server.h"

namespace net {

using request = web_server::http_req_t;
using reply = web_server::http_res_t;

struct route_request : public web_server::http_req_t {
  route_request(request req, boost::urls::url_view const url)
      : web_server::http_req_t{std::move(req)}, url_{url} {}
  boost::urls::url_view url_;
  std::vector<std::string> path_params_;
  std::string username_, password_;
};

template <typename T>
concept JSON = boost::json::has_value_to<T>::value &&
               boost::json::has_value_from<T>::value;

template <typename Fn>
concept StringRouteHandler = requires(std::string_view const& req, Fn f) {
  { f(req) } -> std::convertible_to<std::string>;
};

template <typename Fn>
concept Function = std::is_function_v<Fn>;

template <typename Fn>
concept JsonRouteHandler =
    requires(Fn f, typename utl::first_argument<Fn> arg) {
      { f(arg) } -> JSON;
    };

template <typename Fn>
concept UrlRouteHandler = requires(boost::urls::url_view const& url, Fn f) {
  { f(url) } -> JSON;
};

struct default_exec {
  void exec(auto&& fn, web_server::http_res_cb_t cb) { cb(fn()); }
};

struct asio_exec {
  asio_exec(boost::asio::io_context& io, boost::asio::io_context& worker_pool);

  auto exec(auto&& f, web_server::http_res_cb_t cb) {
    worker_pool_.post([&, f = std::move(f), cb = std::move(cb)]() mutable {
      io_.post([cb = std::move(cb),
                res = std::make_shared<web_server::http_res_t>(f())]() mutable {
        cb(std::move(*res));
      });
    });
  }

  boost::asio::io_context& io_;
  boost::asio::io_context& worker_pool_;
};

using route_request_handler = std::function<reply(route_request, bool)>;
struct handler;

template <typename Executor = default_exec>
struct query_router {
  explicit query_router(Executor&& exec);
  ~query_router();
  query_router(query_router const&);
  query_router(query_router&&);

  query_router& route(std::string method, std::string path_regex,
                      route_request_handler);

  template <StringRouteHandler Fn>
  query_router& route(std::string method, std::string const& path_regex,
                      Fn&& fn) {
    return route(std::move(method), path_regex,
                 [fn = std::forward<Fn>(fn)](web_server::http_req_t const& req,
                                             bool is_ssl) -> reply {
                   try {
                     auto res = net::web_server::string_res_t{
                         boost::beast::http::status::ok, req.version()};
                     res.body() = fn(req.body());
                     res.keep_alive(req.keep_alive());
                     return res;
                   } catch (std::exception const& e) {
                     auto res = net::web_server::empty_res_t{
                         boost::beast::http::status::internal_server_error,
                         req.version()};
                     res.keep_alive(req.keep_alive());
                     return res;
                   }
                 });
  }

  template <JsonRouteHandler Fn>
  query_router& post(std::string const& path_regex, Fn&& fn) {
    return route(
        "POST", path_regex,
        [fn = std::forward<Fn>(fn)](web_server::http_req_t const& req,
                                    bool is_ssl) -> reply {
          try {
            auto res = net::web_server::string_res_t{
                boost::beast::http::status::ok, req.version()};
            res.body() = boost::json::serialize(boost::json::value_from(
                fn(boost::json::value_to<std::decay_t<utl::first_argument<Fn>>>(
                    boost::json::parse(req.body())))));
            res.keep_alive(req.keep_alive());
            return res;
          } catch (std::exception const& e) {
            auto res = net::web_server::empty_res_t{
                boost::beast::http::status::internal_server_error,
                req.version()};
            res.keep_alive(req.keep_alive());
            return res;
          }
        });
  }

  template <UrlRouteHandler Fn>
  query_router& get(std::string const& path_regex, Fn&& fn) {
    namespace http = boost::beast::http;
    namespace json = boost::json;
    return route("GET", path_regex,
                 [fn = std::forward<Fn>(fn)](route_request const& req,
                                             bool const ssl) -> reply {
                   try {
                     auto res = web_server::string_res_t{http::status::ok,
                                                         req.version()};
                     res.set(http::field::content_type, "application/json");
                     res.body() =
                         json::serialize(json::value_from(fn(req.url_)));
                     res.keep_alive(req.keep_alive());
                     return res;
                   } catch (std::exception const& e) {
                     auto res = net::web_server::empty_res_t{
                         boost::beast::http::status::internal_server_error,
                         req.version()};
                     res.keep_alive(req.keep_alive());
                     return res;
                   }
                 });
  }

  void operator()(web_server::http_req_t req, web_server::http_res_cb_t cb,
                  bool is_ssl);
  void reply_hook(std::function<void(reply&)> reply_hook);
  void enable_cors();
  void serve_files(std::filesystem::path const& p);

private:
  static void decode_content(request& req);
  static void set_credentials(route_request& req);

  struct impl;
  std::unique_ptr<impl> impl_;

  Executor exec_;
};

}  // namespace net
