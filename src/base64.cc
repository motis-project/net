#include "net/base64.h"

#include <iostream>
#include <algorithm>
#include <sstream>

#include "boost/archive/iterators/base64_from_binary.hpp"
#include "boost/archive/iterators/binary_from_base64.hpp"
#include "boost/archive/iterators/transform_width.hpp"
#include "boost/archive/iterators/remove_whitespace.hpp"
#include "boost/archive/iterators/ostream_iterator.hpp"

using namespace boost::archive::iterators;

typedef transform_width<
    binary_from_base64<remove_whitespace<std::string::const_iterator>>, 8, 6>
it_binary_t;
typedef base64_from_binary<transform_width<const char *, 6, 8>> base64_text;

namespace net {

// From http://stackoverflow.com/a/10973348
std::string decode_base64(std::string base64) {
  unsigned int padding = count(base64.begin(), base64.end(), '=');

  // replace '=' by base64 encoding of '\0'
  std::replace(base64.begin(), base64.end(), '=', 'A');

  // decode
  std::string result(it_binary_t(base64.begin()), it_binary_t(base64.end()));

  // erase padding '\0' characters
  result.erase(result.end() - padding, result.end());

  return result;
}

// From http://stackoverflow.com/a/7053808
std::string encode_base64(std::string plain) {
  std::stringstream os;
  std::copy(
      base64_text(plain.c_str()),
      base64_text(plain.c_str() + plain.size()),
      ostream_iterator<char>(os)
  );
  return os.str();
}

} // namespace net