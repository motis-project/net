#pragma once

#include <string>
#include <string_view>

#include "boost/beast/http/status.hpp"

#include "net/web_server/web_server.h"

namespace net {

web_server::string_res_t string_response(
    web_server::http_req_t const& req, std::string_view const& text,
    boost::beast::http::status status = boost::beast::http::status::ok,
    std::string_view const& content_type = "text/html");

web_server::string_res_t not_found_response(
    web_server::http_req_t const& req,
    std::string_view const& text = "Not found",
    std::string_view const& content_type = "text/html");

web_server::string_res_t server_error_response(
    web_server::http_req_t const& req,
    std::string_view const& text = "Internal server error",
    std::string_view const& content_type = "text/html");

web_server::string_res_t bad_request_response(
    web_server::http_req_t const& req,
    std::string_view const& text = "Bad request",
    std::string_view const& content_type = "text/html");

web_server::empty_res_t empty_response(
    web_server::http_req_t const& req,
    boost::beast::http::status status = boost::beast::http::status::ok,
    std::string_view const& content_type = "text/html");

}  // namespace net
