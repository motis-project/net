#ifndef NET_BASE64_H_
#define NET_BASE64_H_

#include <string>

namespace net {

std::string decode_base64(std::string base64);
std::string encode_base64(std::string plain);

}  // namespace net

#endif  // HTTP_SERVER_BASE64_DECODE_H_