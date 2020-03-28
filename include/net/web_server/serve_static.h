#pragma once

#include "boost/beast/core/string.hpp"

#include "net/web_server/web_server.h"

namespace net {

bool serve_static_file(boost::beast::string_view doc_root,
                       web_server::http_req_t const& req,
                       web_server::http_res_cb_t const& cb);

}
