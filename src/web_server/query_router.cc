#include "net/web_server/query_router.h"

#include <numeric>
#include <utility>

#include "boost/algorithm/string/predicate.hpp"
#include "boost/lexical_cast.hpp"

#include "net/base64.h"
#include "net/web_server/enable_cors.h"
#include "net/web_server/responses.h"
#include "net/web_server/url_decode.h"

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
        [](route_request const& req, const web_server::http_res_cb_t& cb,
           bool) { return cb(empty_response(req)); });
}

void query_router::operator()(web_server::http_req_t req,
                              web_server::http_res_cb_t const& cb,
                              bool is_ssl) {
  std::cmatch match;
  auto route = std::find_if(
      std::begin(routes_), std::end(routes_),
      [&match, &req](handler const& route) {
        auto const target =
            std::string{req.target().data(), req.target().size()};
        return (route.method_ == "*" || route.method_ == req.method_string()) &&
               std::regex_match(target.c_str(), match, route.path_);
      });

  if (route == std::end(routes_)) {
    auto rep = reply{not_found_response(req)};
    if (reply_hook_) {
      reply_hook_(rep);
    }
    return cb(std::move(rep));
  }

  route_request route_req(req);
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

    size_t split = credentials.find_first_of(':');
    if (split == std::string::npos) {
      return;
    }

    req.username_ = credentials.substr(0, split);
    req.password_ = credentials.substr(split + 1, std::string::npos);
  }
}

}  // namespace net