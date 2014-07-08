//
// request_handler.hpp
// ~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef HTTP_REQUEST_HANDLER_HPP
#define HTTP_REQUEST_HANDLER_HPP

#include <string>
#include <functional>

#include "net/http/server/reply.hpp"

namespace net {
namespace http {
namespace server {

struct request;

typedef std::function<void (reply)> callback;
typedef std::function<void (const request&, callback)> request_handler;

} // namespace server
} // namespace http
} // namespace net

#endif // HTTP_REQUEST_HANDLER_HPP
