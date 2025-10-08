#pragma once

#include <memory>

#include "net/web_server/web_server.h"

namespace net {

struct lb {
  lb(boost::asio::io_context&, std::string const& url,
     web_server::http_req_cb_t);
  lb(lb&&);
  lb& operator=(lb&&);
  ~lb();

  void run() const;
  void stop() const;

  struct impl;
  std::unique_ptr<impl> impl_;
};

}  // namespace net