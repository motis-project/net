#pragma once

#include <filesystem>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#include "boost/beast/http/status.hpp"
#include "boost/json.hpp"
#include "boost/url/url_view.hpp"

#include "net/web_server/web_server.h"

#include "utl/overloaded.h"

namespace net {

using request = web_server::http_req_t;
using reply = web_server::http_res_t;

struct route_request : public web_server::http_req_t {
  route_request(request req, boost::urls::url_view const url)
      : web_server::http_req_t(req), url_{url} {}
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
  { f(url) } -> std::convertible_to<boost::json::value>;
};

struct query_router {
  using route_request_handler = std::function<void(
      route_request const&, web_server::http_res_cb_t, bool)>;

  query_router& route(std::string method, std::string const& path_regex,
                      route_request_handler handler);

  template <StringRouteHandler Fn>
  query_router& route(std::string method, std::string const& path_regex,
                      Fn&& fn) {
    return route(std::move(method), path_regex,
                 [fn = std::forward<Fn>(fn)](
                     web_server::http_req_t req,
                     web_server::http_res_cb_t const& cb, bool is_ssl) {
                   try {
                     auto res = net::web_server::string_res_t{
                         boost::beast::http::status::ok, req.version()};
                     res.body() = fn(req.body());
                     res.keep_alive(req.keep_alive());
                     cb(res);
                   } catch (std::exception const& e) {
                     std::cout << "exception: " << e.what() << "\n";
                     auto res = net::web_server::empty_res_t{
                         boost::beast::http::status::internal_server_error,
                         req.version()};
                     res.keep_alive(req.keep_alive());
                     cb(res);
                   }
                 });
  }

  template <JsonRouteHandler Fn>
  query_router& route(std::string method, std::string const& path_regex,
                      Fn&& fn) {
    return route(
        std::move(method), path_regex,
        [fn = std::forward<Fn>(fn)](web_server::http_req_t req,
                                    web_server::http_res_cb_t const& cb,
                                    bool is_ssl) {
          try {
            auto res = net::web_server::string_res_t{
                boost::beast::http::status::ok, req.version()};
            res.body() = boost::json::serialize(boost::json::value_from(
                fn(boost::json::value_to<std::decay_t<utl::first_argument<Fn>>>(
                    boost::json::parse(req.body())))));
            res.keep_alive(req.keep_alive());
            cb(res);
          } catch (std::exception const& e) {
            std::cout << "exception: " << e.what() << "\n";
            auto res = net::web_server::empty_res_t{
                boost::beast::http::status::internal_server_error,
                req.version()};
            res.keep_alive(req.keep_alive());
            cb(res);
          }
        });
  }

  template <UrlRouteHandler Fn>
  query_router& route(std::string method, std::string const& path_regex,
                      Fn&& fn) {
    return route(std::move(method), path_regex,
                 [fn = std::forward<Fn>(fn)](
                     route_request req, web_server::http_res_cb_t const& cb,
                     bool is_ssl) {
                   try {
                     auto res = net::web_server::string_res_t{
                         boost::beast::http::status::ok, req.version()};
                     res.body() = boost::json::serialize(fn(req.url_));
                     res.keep_alive(req.keep_alive());
                     cb(res);
                   } catch (std::exception const& e) {
                     std::cout << "exception: " << e.what() << "\n";
                     auto res = net::web_server::empty_res_t{
                         boost::beast::http::status::internal_server_error,
                         req.version()};
                     res.keep_alive(req.keep_alive());
                     cb(res);
                   }
                 });
  }

  void operator()(web_server::http_req_t, web_server::http_res_cb_t const&,
                  bool);
  void reply_hook(std::function<void(web_server::http_res_t&)> reply_hook);
  void enable_cors();
  void serve_files(std::filesystem::path const&);

private:
  struct handler {
    std::string method_;
    std::regex path_;
    route_request_handler request_handler_;
  };

  static void decode_content(request& req);

  static void set_credentials(route_request& req);

  std::vector<handler> routes_;
  std::function<void(reply&)> reply_hook_;
};

}  // namespace net