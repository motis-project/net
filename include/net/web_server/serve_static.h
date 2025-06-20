#pragma once

#include <filesystem>

#include "boost/beast/core/string.hpp"

#include "net/web_server/web_server.h"

namespace net {

std::optional<web_server::http_res_t> serve_static_file(
    std::filesystem::path const& doc_root, web_server::http_req_t const& req);

std::optional<web_server::http_res_t> serve_static_file(
    boost::beast::string_view doc_root, web_server::http_req_t const& req);

}  // namespace net
