//
// url_decode.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Extracted from request_handler.hpp by Felix Guendling
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_URL_DECODE_HPP
#define HTTP_URL_DECODE_HPP

#include <string>

namespace net {
namespace http {
namespace server {

bool url_decode(const std::string& in, std::string& out);

} // namespace server
} // namespace http
} // namespace net

#endif // HTTP_URL_DECODE_HPP
