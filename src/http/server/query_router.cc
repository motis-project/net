#include "net/http/server/query_router.hpp"

#include "boost/algorithm/string/predicate.hpp"
#include "boost/lexical_cast.hpp"

#include "net/base64.h"
#include "net/http/server/url_decode.hpp"
#include "net/http/server/enable_cors.hpp"

namespace net {
namespace http {
namespace server {

query_router& query_router::route(std::string method,
                                  std::string path_regex,
                                  route_request_handler handler) {
  routes_.push_back({method, boost::regex(path_regex), handler});
  return *this;
}

void query_router::reply_hook(std::function<void (reply&)> reply_hook) {
  reply_hook_ = std::move(reply_hook);
}

void query_router::enable_cors() {
  reply_hook([](reply& rep) { struct enable_cors cors(rep); });
  route("OPTIONS", ".*", [](request const&, callback cb) {
    reply rep;
    rep.status = reply::ok;
    return cb(rep);
  });
}

void query_router::operator()(request const& req, callback cb) {
  boost::cmatch match;
  auto route = std::find_if(std::begin(routes_), std::end(routes_),
                            [&match, &req](handler const& route) {
    return (route.method == "*" || route.method == req.method) &&
           boost::regex_match(req.uri.c_str(), match, route.path);
  });

  if (route == std::end(routes_)) {
    auto rep = reply::stock_reply(reply::not_found);
    if (reply_hook_) {
      reply_hook_(rep);
    }
    return cb(rep);
  }

  route_request route_req(std::move(req));
  for (unsigned i = 1; i < match.size(); ++i) {
    route_req.path_params.push_back(match[i]);
  }

  set_credentials(route_req);
  decode_content(route_req);

  try {
    return route->request_handler(route_req, [cb, this](reply rep) {
      set_content_length(rep);
      set_content_type(rep);
      set_status(rep);
      if (reply_hook_) {
        reply_hook_(rep);
      }
      return cb(rep);
    });
  } catch (std::exception const& e) {
    reply rep;
    rep.content = std::string("{\"error\": \"") + e.what() + "\"}";
    rep.status = reply::internal_server_error;
    if (reply_hook_) {
      reply_hook_(rep);
    }
    return cb(rep);
 } catch (...) {
   auto rep = reply::stock_reply(reply::internal_server_error);
   if (reply_hook_) {
     reply_hook_(rep);
   }
   return cb(rep);
 }
}

void query_router::decode_content(request& req) {
  bool urlencoded = false;
  for (const auto& h : req.headers) {
    if (h.name == "Content-Type" &&
        h.value.find("urlencoded") != std::string::npos) {
      urlencoded = true;
    }
  }

  if (urlencoded) {
    std::string decoded_content;
    url_decode(req.content, decoded_content);
    req.content = decoded_content;
  }
}

void query_router::set_credentials(route_request& req) {
  std::string auth;
  bool auth_set = false;
  for (const auto& h : req.headers) {
    if (h.name == "Authorization") {
      auth = h.value;
      auth_set = true;
      break;
    }
  }

  if (!auth_set || auth.length() <= 6) {
    return;
  }

  std::string credentials = decode_base64(auth.substr(6));

  size_t split = credentials.find_first_of(":");
  if (split == std::string::npos) {
    return;
  }

  req.username = credentials.substr(0, split);
  req.password = credentials.substr(split + 1, std::string::npos);
}

void query_router::set_content_length(reply& rep) {
  if (rep.content.length() == 0) {
    return;
  }

  auto content_length_header = get_header(rep, "Content-Length");
  if (content_length_header != std::end(rep.headers)) {
    return;
  }

  auto content_length = boost::lexical_cast<std::string>(rep.content.length());
  rep.headers.push_back({ "Content-Length", std::move(content_length) });
}

void query_router::set_content_type(reply& rep) {
  if (rep.content.length() == 0) {
    return;
  }

  auto content_length_header = get_header(rep, "Content-Type");
  if (content_length_header != std::end(rep.headers)) {
    return;
  }

  if (boost::starts_with(rep.content, "<?xml")) {
    rep.headers.push_back({ "Content-Type", "text/xml; charset=utf-8" });
  } else {
    rep.headers.push_back({ "Content-Type", "text/txt; charset=utf-8" });
  }
}

void query_router::set_status(reply& rep) {
  if (rep.status == 0) {
    rep.status = reply::ok;
  }
}

auto query_router::get_header(reply const& rep, std::string const& key)
-> decltype(reply::headers)::const_iterator {
  return std::find_if(std::begin(rep.headers), std::end(rep.headers),
                      [&key](header const& hdr) {
    return hdr.name == key;
  });
}

} // namespace server
} // namespace http
} // namespace net
