#include "net/web_server/responses.h"

#include "boost/beast/version.hpp"

namespace http = boost::beast::http;

namespace net {

web_server::string_res_t string_response(
    web_server::http_req_t const& req, std::string_view const& text,
    http::status status, std::string_view const& content_type) {
  web_server::string_res_t res{status, req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, content_type);
  res.keep_alive(req.keep_alive());
  res.body() = text;
  res.prepare_payload();
  return res;
}

web_server::string_res_t not_found_response(
    web_server::http_req_t const& req, std::string_view const& text,
    std::string_view const& content_type) {
  return string_response(req, text, http::status::not_found, content_type);
}

web_server::string_res_t server_error_response(
    web_server::http_req_t const& req, std::string_view const& text,
    std::string_view const& content_type) {
  return string_response(req, text, http::status::internal_server_error,
                         content_type);
}

web_server::empty_res_t empty_response(
    web_server::http_req_t const& req, boost::beast::http::status status ,
    std::string_view const& content_type) {
  web_server::empty_res_t res{status, req.version()};
  res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
  res.set(http::field::content_type, content_type);
  res.keep_alive(req.keep_alive());
  return res;
}

}  // namespace net
