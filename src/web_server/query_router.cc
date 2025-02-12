#include "net/web_server/query_router.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "fmt/base.h"

#include "boost/algorithm/string/predicate.hpp"
#include "boost/beast/http/status.hpp"
#include "boost/json.hpp"
#include "boost/url.hpp"
#include "boost/url/url_view.hpp"

#include "utl/helpers/algorithm.h"
#include "utl/overloaded.h"

#include "openapi/missing_param_exception.h"

#include "net/base64.h"
#include "net/web_server/enable_cors.h"
#include "net/web_server/responses.h"
#include "net/web_server/serve_static.h"
#include "net/web_server/url_decode.h"
#include "net/web_server/web_server.h"

namespace net {

struct header {
  std::string key_;
  std::string value_;
};

struct handler {
  std::string method_;
  std::string prefix_;
  route_request_handler request_handler_;
};

asio_exec::asio_exec(boost::asio::io_context& io,
                     boost::asio::io_context& worker_pool)
    : io_{io}, worker_pool_{worker_pool} {}

template <typename Executor>
struct query_router<Executor>::impl {
  std::vector<header> headers_;
  std::vector<handler> routes_;
  std::function<void(reply&)> reply_hook_;
};

template <typename Executor>
query_router<Executor>::query_router(Executor&& exec)
    : impl_{std::make_unique<impl>()}, exec_{std::move(exec)} {}

template <typename Executor>
query_router<Executor>::query_router(query_router<Executor>&&) = default;

template <typename Executor>
query_router<Executor>::query_router(query_router<Executor> const& o)
    : impl_{std::make_unique<impl>(*o.impl_)}, exec_{o.exec_} {}

template <typename Executor>
query_router<Executor>::~query_router() = default;

template <typename Executor>
query_router<Executor>& query_router<Executor>::route(std::string method,
                                                      std::string prefix,
                                                      route_request_handler h) {
  impl_->routes_.push_back(
      {std::move(method), std::move(prefix), std::move(h)});
  return *this;
}

template <typename Executor>
void query_router<Executor>::operator()(web_server::http_req_t req,
                                        web_server::http_res_cb_t cb,
                                        bool is_ssl) {
  auto const url = boost::urls::url_view{req.target()};
  auto const path = url.path();

  auto route = utl::find_if(impl_->routes_, [&](handler const& h) {
    return (h.method_ == "*" || h.method_ == req.method_string()) &&
           path.starts_with(h.prefix_);
  });

  if (route == end(impl_->routes_)) {
    auto rep = reply{not_found_response(req)};
    if (impl_->reply_hook_) {
      impl_->reply_hook_(rep);
    }
    return cb(std::move(rep));
  }

  auto route_req = route_request{std::move(req), url};

  set_credentials(route_req);
  decode_content(route_req);

  return exec_.exec(
      [this, route, is_ssl, req = std::move(route_req)]() {
        reply rep;
        try {
          rep = route->request_handler_(req, is_ssl);
        } catch (openapi::missing_param_exception const& e) {
          using namespace boost::json;
          rep = bad_request_response(
              req,
              serialize(value{
                  {"error", fmt::format("missing parameter: {}", e.param_)}}));
        } catch (std::exception const& e) {
          using namespace boost::json;
          rep =
              server_error_response(req, serialize(value{{"error", e.what()}}));
        } catch (...) {
          rep = reply{server_error_response(req)};
        }
        if (impl_->reply_hook_) {
          try {
            impl_->reply_hook_(rep);
          } catch (...) {
            std::cerr << "query_router: unhandled exception in reply hook\n";
          }
        }
        // Add headers
        std::visit(
            [&](auto& r) {
              for (auto const& header : impl_->headers) {
                r.set(header.key_, header.value_);
              }
            },
            rep);
        return std::move(rep);
      },
      std::move(cb));
}

template <typename Executor>
void query_router<Executor>::reply_hook(
    std::function<void(reply&)> reply_hook) {
  impl_->reply_hook_ = std::move(reply_hook);
}

template <typename Executor>
void query_router<Executor>::enable_cors() {
  reply_hook([](reply& rep) { net::enable_cors(rep); });
  route("OPTIONS", "",
        [](route_request const& req, bool) { return empty_response(req); });
}

template <typename Executor>
void query_router<Executor>::add_header(std::string key, std::string value) {
  impl_->headers_.emplace_back(std::move(key), std::move(value));
}

template <typename Executor>
void query_router<Executor>::serve_files(std::filesystem::path const& p) {
  route("GET", "",
        [p](route_request const& req, bool) -> web_server::http_res_t {
          if (auto res = serve_static_file(p.generic_string(), req);
              res.has_value()) {
            return std::move(*res);
          } else {
            namespace http = boost::beast::http;
            return net::web_server::string_res_t{http::status::not_found,
                                                 req.version()};
          }
        });
}

template <typename Executor>
void query_router<Executor>::decode_content(request& req) {
  if (auto const it = req.base().find(boost::beast::http::field::content_type);
      it != end(req.base()) &&
      it->value().find("urlencoded") != std::string_view::npos) {
    std::string decoded_content;
    url_decode(req.body(), decoded_content);
    req.body() = decoded_content;
  }
}

template <typename Executor>
void query_router<Executor>::set_credentials(route_request& req) {
  if (auto const it = req.base().find(boost::beast::http::field::authorization);
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

template struct query_router<default_exec>;
template struct query_router<asio_exec>;
template struct query_router<fiber_exec>;

}  // namespace net