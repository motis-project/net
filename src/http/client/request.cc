#include "net/http/client/request.h"

#include <sstream>

namespace net::http::client {

static char const* const method_to_string[] = {"GET", "POST", "DELETE", "PUT",
                                               "OPTIONS"};

request::request(url addr, enum method m, request::str_map hdr,
                 std::string body)
    : address(std::move(addr)),
      req_method(m),
      headers(std::move(hdr)),
      body(std::move(body)) {}

request::request(const char* s) : request(std::string{s}) {}

std::string request::to_str() const {
  std::stringstream request_stream;

  request_stream << method_to_string[static_cast<int>(req_method)];
  request_stream << " " << address.path();
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