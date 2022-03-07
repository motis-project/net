#include "net/http/client/url.h"

#include <iostream>
#include <stdexcept>
#include <utility>

#include "boost/regex.hpp"

namespace net::http::client {

static boost::regex const url_regex(
    "(.*://)([a-zA-Z0-9\\.\\-]+)(:[0-9]*)?(.*)");

url::url(std::string const& url) : str_(url) {
  // Extract protocol, port, host address and path from the URL.
  boost::match_results<std::string::const_iterator> what;
  bool matches = boost::regex_search(url, what, url_regex);

  // Throw invalid_argument exception if the regular expression didn't match.
  if (!matches) {
    throw std::invalid_argument(url + " is not a valid URL");
  }

  // Extract match.
  host_ = what[2].str();
  port_ = what[3].str();
  path_ = what[4].str();

  // Set port and path to default values if not set explicitly.
  // Also cut leading ':' from port if set explicitly.
  std::string prot = what[1].str();
  if (!prot.empty()) {
    prot = prot.substr(0, prot.length() - 3);
  }
  port_ = port_.empty() ? prot : port_.substr(1, port_.length() - 1);
  path_ = path_.empty() ? "/" : path_;
  prot_ = prot;
}

url::url(char const* address) : url(std::string(address)) {}

}  // namespace net::http::client
