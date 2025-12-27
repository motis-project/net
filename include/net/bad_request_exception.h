#pragma once

#include <stdexcept>

namespace net {
struct bad_request_exception : public std::runtime_error {
  using std::runtime_error::runtime_error;
};
}  // namespace net