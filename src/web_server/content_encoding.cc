#include "net/web_server/content_encoding.h"

#include <cstdlib>
#include <algorithm>
#include <locale>
#include <sstream>

#include "boost/beast/http/rfc7230.hpp"

#include "boost/iostreams/device/back_inserter.hpp"
#include "boost/iostreams/filter/gzip.hpp"
#include "boost/iostreams/filtering_stream.hpp"

namespace bio = boost::iostreams;
namespace http = boost::beast::http;

namespace net {

http_content_encoding select_content_encoding(
    boost::beast::string_view const accept_encoding) {
  auto const is_acceptable = [](http::param_list const& params) {
    if (auto const it =
            std::find_if(params.begin(), params.end(),
                         [](auto const& param) { return param.first == "q"; });
        it != params.end() && !it->second.empty()) {
      std::stringstream str;
      str.imbue(std::locale::classic());
      auto val = 0.0;
      str << it->second;
      str >> val;
      return val != 0.0;
    }
    return true;
  };
  auto any_acceptable = false;
  for (auto const& ext : http::ext_list{accept_encoding}) {
    if (ext.first == "gzip") {
      return is_acceptable(ext.second) ? http_content_encoding::GZIP
                                       : http_content_encoding::IDENTITY;
    } else if (ext.first == "*") {
      any_acceptable = is_acceptable(ext.second);
    }
  }
  return any_acceptable ? http_content_encoding::GZIP
                        : http_content_encoding::IDENTITY;
}

std::string gzip_content(
    std::string_view const content,
    bio::gzip_params const& gzip_params = bio::gzip::default_compression) {
  auto compressed = std::string{};
  auto sink = bio::back_inserter(compressed);
  auto stream = bio::filtering_ostream{};
  stream.push(bio::gzip_compressor{gzip_params});
  stream.push(sink);
  stream << content;
  stream.pop();
  return compressed;
}

void set_response_body(web_server::string_res_t& res,
                       http_content_encoding const encoding,
                       std::string_view const content) {
  switch (encoding) {
    case http_content_encoding::IDENTITY:
      res.body() = std::string{content};
      break;
    case http_content_encoding::GZIP:
      res.set(http::field::content_encoding, "gzip");
      res.body() = gzip_content(content);
      break;
  }
}

void set_response_body(web_server::string_res_t& res,
                       web_server::http_req_t const& req,
                       std::string_view const content) {
  set_response_body(
      res, select_content_encoding(req[http::field::accept_encoding]), content);
}

}  // namespace net
