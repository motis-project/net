#pragma once

#include <stdexcept>

namespace net {
struct not_found_exception : public std::runtime_error {
  using std::runtime_error::runtime_error;
};
}  // namespace net