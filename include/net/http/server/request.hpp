//
// request.hpp
// ~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <vector>

#include "net/http/server/header.hpp"

namespace net {
namespace http {
namespace server {

/// A request received from a client.
struct request
{
  request()
    : http_version_major(0),
      http_version_minor(0),
      content_length(0) {
  }
  std::string method;
  std::string uri;
  int http_version_major;
  int http_version_minor;
  std::vector<header> headers;
  std::string content;
  std::size_t content_length;
};

} // namespace server
} // namespace http
} // namespace net

#endif // HTTP_REQUEST_HPP
