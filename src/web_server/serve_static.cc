#include "net/web_server/serve_static.h"

#include <filesystem>
#include <string_view>

#include "boost/url.hpp"

#include "net/web_server/responses.h"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace fs = std::filesystem;

namespace net {

inline std::string_view mime_type(beast::string_view ext) {
  using beast::iequals;
  if (iequals(ext, ".js") || iequals(ext, ".mjs")) {
    return "application/javascript";
  }
  if (iequals(ext, ".wasm")) {
    return "application/wasm";
  }
  if (iequals(ext, ".css")) {
    return "text/css";
  }
  if (iequals(ext, ".html") || iequals(ext, ".htm")) {
    return "text/html";
  }
  if (iequals(ext, ".txt")) {
    return "text/plain";
  }
  if (iequals(ext, ".json")) {
    return "application/json";
  }
  if (iequals(ext, ".xml")) {
    return "application/xml";
  }
  if (iequals(ext, ".png")) {
    return "image/png";
  }
  if (iequals(ext, ".jpg") || iequals(ext, ".jpeg") || iequals(ext, ".jpe")) {
    return "image/jpeg";
  }
  if (iequals(ext, ".gif")) {
    return "image/gif";
  }
  if (iequals(ext, ".webp")) {
    return "image/webp";
  }
  if (iequals(ext, ".ico")) {
    return "image/vnd.microsoft.icon";
  }
  if (iequals(ext, ".svg")) {
    return "image/svg+xml";
  }
  if (iequals(ext, ".svgz")) {
    return "image/svg+xml";
  }
  if (iequals(ext, ".eot")) {
    return "application/vnd.ms-fontobject";
  }
  if (iequals(ext, ".otf")) {
    return "font/otf";
  }
  if (iequals(ext, ".ttf")) {
    return "font/ttf";
  }
  if (iequals(ext, ".pdf")) {
    return "application/pdf";
  }
  return " application/octet-stream";
}

std::optional<web_server::http_res_t> handle_directory_redirect(
    fs::path const& path, web_server::http_req_t const& req,
    boost::urls::url_view const& url) {
  boost::system::error_code sys_ec;
  auto const file_status = fs::status(path, sys_ec);
  if (!sys_ec.failed() && fs::is_directory(file_status)) {
    auto new_target = boost::urls::url{url};
    new_target.set_path(url.path() + "/");
    return moved_response(req,
                          static_cast<boost::core::string_view>(new_target));
  }
  return std::nullopt;
}

std::optional<web_server::http_res_t> serve_static_file(
    fs::path const& doc_root, web_server::http_req_t const& req) {
  if (req.method() != http::verb::get && req.method() != http::verb::head) {
    return bad_request_response(req, "Invalid method");
  }

  auto const url = boost::urls::url_view{req.target()};
  auto const url_path = url.path();

  auto path = doc_root;
  for (auto const& seg : url.segments()) {
    if (seg.empty() || seg == "." || seg == ".." ||
        seg.find(":") != std::string::npos) {
      return bad_request_response(req, "Invalid target");
    }
    path /= std::u8string{seg.begin(), seg.end()};
  }
  if (url_path.back() == '/') {
    path /= "index.html";
  }

  if (auto res = handle_directory_redirect(path, req, url); res.has_value()) {
    return res;
  }

  boost::beast::error_code ec;
  http::file_body::value_type body;
  body.open(reinterpret_cast<char const*>(path.u8string().c_str()),
            beast::file_mode::scan, ec);

  if (ec == beast::errc::no_such_file_or_directory) {
    return std::nullopt;
  } else if (ec) {
    return server_error_response(req, ec.message());
  }

  auto const size = body.size();
  auto const ext = path.extension().string();
  auto const content_type = mime_type(ext);

  if (req.method() == http::verb::head) {
    auto res = empty_response(req, http::status::ok, content_type);
    res.content_length(size);
    return res;
  } else {
    auto res = web_server::file_res_t{
        std::piecewise_construct, std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::content_type, content_type);
    res.content_length(size);
    return res;
  }
}

std::optional<web_server::http_res_t> serve_static_file(
    beast::string_view doc_root, web_server::http_req_t const& req) {
  return serve_static_file(fs::path{static_cast<std::string_view>(doc_root)},
                           req);
}

}  // namespace net
