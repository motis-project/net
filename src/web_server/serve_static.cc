#include "net/web_server/serve_static.h"

#include <string_view>

#include "boost/filesystem.hpp"

#include "net/web_server/responses.h"

namespace beast = boost::beast;
namespace http = boost::beast::http;
namespace fs = boost::filesystem;

namespace net {

inline std::string_view mime_type(beast::string_view path) {
  using beast::iequals;
  auto const ext = [&path] {
    auto const pos = path.rfind('.');
    if (pos == beast::string_view::npos) {
      return beast::string_view{};
    }
    return path.substr(pos);
  }();
  if (iequals(ext, ".js")) {
    return "application/javascript";
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

std::string path_cat(beast::string_view base, beast::string_view path) {
  if (base.empty()) {
    return std::string(path);
  }
  std::string result(base);
#ifdef BOOST_MSVC
  char constexpr path_separator = '\\';
  if (result.back() == path_separator) {
    result.resize(result.size() - 1);
  }
  result.append(path.data(), path.size());
  for (auto& c : result) {
    if (c == '/') {
      c = path_separator;
    }
  }
#else
  char constexpr path_separator = '/';
  if (result.back() == path_separator) {
    result.resize(result.size() - 1);
  }
  result.append(path.data(), path.size());
#endif
  return result;
}

std::string add_trailing_slash(std::string_view target) {
  std::string new_target;
  new_target.reserve(target.size() + 1);
  auto const question_mark_pos = target.find('?');
  new_target += target.substr(0, question_mark_pos);
  new_target += '/';
  if (question_mark_pos != std::string_view::npos) {
    new_target += target.substr(question_mark_pos);
  }
  return new_target;
}

bool handle_directory_redirect(std::string const& path,
                               web_server::http_req_t const& req,
                               web_server::http_res_cb_t const& cb) {
  boost::system::error_code sys_ec;
  auto const file_status = fs::status(fs::path{path}, sys_ec);
  if (!sys_ec.failed() && fs::is_directory(file_status)) {
    cb(moved_response(req, add_trailing_slash(req.target())));
    return true;
  }
  return false;
}

bool serve_static_file(beast::string_view doc_root,
                       web_server::http_req_t const& req,
                       web_server::http_res_cb_t const& cb) {
  if (req.method() != http::verb::get && req.method() != http::verb::head) {
    cb(bad_request_response(req, "Invalid method"));
    return true;
  }

  auto const target = [&]() {
    auto const t = req.target();
    auto const question_mark_pos = t.find('?');
    return question_mark_pos == std::string_view::npos
               ? t
               : t.substr(0, question_mark_pos);
  }();
  if (target.empty() || target[0] != '/' ||
      target.find("..") != beast::string_view::npos) {
    cb(bad_request_response(req, "Invalid target"));
    return true;
  }

  auto path = path_cat(doc_root, target);
  if (target.back() == '/') {
    path.append("index.html");
  }

  if (handle_directory_redirect(path, req, cb)) {
    return true;
  }

  boost::beast::error_code ec;
  http::file_body::value_type body;
  body.open(path.c_str(), beast::file_mode::scan, ec);

  if (ec == beast::errc::no_such_file_or_directory) {
    return false;
  } else if (ec) {
    cb(server_error_response(req, ec.message()));
    return true;
  }

  auto const size = body.size();
  auto const content_type = mime_type(path);

  if (req.method() == http::verb::head) {
    auto res = empty_response(req, http::status::ok, content_type);
    res.content_length(size);
    cb(res);
    return true;
  } else {
    auto res = web_server::file_res_t{
        std::piecewise_construct, std::make_tuple(std::move(body)),
        std::make_tuple(http::status::ok, req.version())};
    res.set(http::field::content_type, content_type);
    res.content_length(size);
    cb(std::move(res));
    return true;
  }
}

}  // namespace net
