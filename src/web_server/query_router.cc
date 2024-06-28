#include "net/web_server/query_router.h"

#include <net/web_server/serve_static.h>
#include <utility>

#include "boost/algorithm/string/predicate.hpp"
#include "boost/url.hpp"

#include "net/base64.h"
#include "net/web_server/enable_cors.h"
#include "net/web_server/responses.h"
#include "net/web_server/url_decode.h"

namespace url = boost::urls;

namespace net {

query_router& query_router::route(std::string method,
                                  std::string const& path_regex,
                                  route_request_handler handler) {
  routes_.push_back(
      {std::move(method), std::regex(path_regex), std::move(handler)});
  return *this;
}

void query_router::reply_hook(std::function<void(reply&)> reply_hook) {
  reply_hook_ = std::move(reply_hook);
}

void query_router::enable_cors() {
  reply_hook([](reply& rep) { net::enable_cors(rep); });
  route("OPTIONS", ".*",
        [](route_request const& req, web_server::http_res_cb_t const& cb,
           bool) { return cb(empty_response(req)); });
}

void query_router::serve_files(std::filesystem::path const& p) {
  route(
      "GET", ".*",
      [p](route_request const& req, web_server::http_res_cb_t const& cb, bool) {
        if (serve_static_file(p.generic_string(), req, cb)) {
          return;
        } else {
          namespace http = boost::beast::http;
          cb(net::web_server::string_res_t{http::status::not_found,
                                           req.version()});
          return;
        }
      });
}

void query_router::operator()(web_server::http_req_t req,
                              web_server::http_res_cb_t const& cb,
                              bool is_ssl) {
  auto const url = url::url_view{req.target()};
  auto const path = url.path();

  auto match = std::cmatch{};
  auto route =
      std::find_if(begin(routes_), end(routes_), [&](handler const& h) {
        return (h.method_ == "*" || h.method_ == req.method_string()) &&
               std::regex_match(path.c_str(), path.c_str() + path.size(), match,
                                h.path_);
      });

  if (route == end(routes_)) {
    auto rep = reply{not_found_response(req)};
    if (reply_hook_) {
      reply_hook_(rep);
    }
    return cb(std::move(rep));
  }

  route_request route_req(req, url);
  for (unsigned i = 1; i < match.size(); ++i) {
    route_req.path_params_.push_back(match[i]);
  }

  set_credentials(route_req);
  decode_content(route_req);

  try {
    return route->request_handler_(
        route_req,
        [cb, this](reply rep) {
          if (reply_hook_) {
            reply_hook_(rep);
          }
          return cb(std::move(rep));
        },
        is_ssl);
  } catch (std::exception const& e) {
    auto rep = reply{server_error_response(
        req, std::string(R"({"error": ")") + e.what() + "\"}")};
    if (reply_hook_) {
      reply_hook_(rep);
    }
    return cb(std::move(rep));
  } catch (...) {
    auto rep = reply{server_error_response(req)};
    if (reply_hook_) {
      reply_hook_(rep);
    }
    return cb(std::move(rep));
  }
}

void query_router::decode_content(request& req) {
  if (auto const it = req.base().find(boost::beast::http::field::content_type);
      it != end(req.base()) &&
      it->value().find("urlencoded") != std::string_view::npos) {
    std::string decoded_content;
    url_decode(req.body(), decoded_content);
    req.body() = decoded_content;
  }
}

void query_router::set_credentials(route_request& req) {
  if (auto const it = req.base().find(boost::beast::http::field::authorization);
      it != req.base().end()) {
    auto const auth = it->value().substr(6);
    auto const credentials =
        decode_base64(std::string{auth.data(), auth.size()});

    std::size_t const split = credentials.find_first_of(':');
    if (split == std::string::npos) {
      return;
    }

    req.username_ = credentials.substr(0, split);
    req.password_ = credentials.substr(split + 1, std::string::npos);
  }
}

}  // namespace net