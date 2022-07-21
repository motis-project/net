#include "net/http/client/request.h"

#include "boost/algorithm/string/predicate.hpp"

#include <sstream>

namespace net::http::client {

static char const* const method_to_string[] = {"GET", "POST", "DELETE", "PUT",
                                               "OPTIONS"};

char const* method_to_str(enum request::method m) {
  return method_to_string[m];
}

request::request(url addr, enum method m, request::str_map hdr,
                 std::string body)
    : address(std::move(addr)),
      req_method(m),
      headers(std::move(hdr)),
      body(std::move(body)) {}

request::request(std::string const& u) : request{url{u}} {}

request::request(const char* s) : request(std::string{s}) {}

url request::peer() const { return proxy.has_value() ? *proxy : address; }

bool request::use_https() const {
  auto const is_https = [](url const& u) {
    return boost::algorithm::starts_with(u.prot(), "https") ||
           u.port() == "443";
  };
  return (proxy.has_value() && is_https(*proxy)) || is_https(address);
}

bool request::use_http() const {
  auto const is_https = [](url const& u) {
    return u.prot() == "http" || u.port() == "80";
  };
  return (proxy.has_value() && is_https(*proxy)) || is_https(address);
}

request request::set_proxy(url const& u) {
  auto cpy = *this;
  if (!u.empty()) {
    cpy.proxy = u;
  }
  return cpy;
}

std::string request::to_str() const {
  std::stringstream request_stream;

  request_stream << method_to_string[static_cast<int>(req_method)];
  request_stream << " " << (proxy.has_value() ? address.str() : address.path());
  request_stream << " HTTP/1.1\r\n";
  request_stream << "Host: " << address.host() << "\r\n";

  for (auto const& header : headers) {
    request_stream << header.first << ": " << header.second << "\r\n";
  }

  if (!body.empty()) {
    auto len_str = std::to_string(body.length());
    request_stream << "Content-Length: " + len_str + "\r\n";
  }

  request_stream << "\r\n";
  request_stream << body;

  return request_stream.str();
}

}  // namespace net::http::client
