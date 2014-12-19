#include <iostream>
#include "net/http/client/https_client.h"

using namespace net::http::client;

// CMakeLists.txt example (i.e. for GCC/Clang)
/*

cmake_minimum_required(VERSION 2.6)
project(request)

add_subdirectory(net EXCLUDE_FROM_ALL)
include_directories(net/include)
add_executable(request main.cc)
target_link_libraries(request net-https_client)
set_target_properties(request PROPERTIES COMPILE_FLAGS "-std=c++11")

*/

// Turn off decompression support (eliminates zlib dependency):
// cmake -DNO_ZLIB:bool=true ..


int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cout << "usage: " << argv[0] << " URL\n";
    return 0;
  }

  // Boost Asio IO Service object
  // Represents an 'event loop' for asynchronous Input/Output operations
  // (such as networking or timers)
  boost::asio::io_service ios;

  // Request:
  // URL     [mandatory, i.e. "http://www.google.de"]
  // method  [optional, default = HTTP GET],
  // headers [optional, default = empty],
  // body    [optional, default = empty, does only make sense for POST/PUT/...]
  request req {
    argv[1],
    request::method::GET,
    {
      // Sample headers:
      // Mircorsoft Windows 7 using Internet Explorer 8
      // (omit the headers if not needed)
      { "Accept", "application/x-ms-application, image/jpeg, application/xaml+xml, image/gif, image/pjpeg, application/x-ms-xbap, */*" },
      { "Accept-Language", "de-DE" },
      { "User-Agent", "Mozilla/4.0 (compatible; MSIE 8.0; Windows NT 6.1; WOW64; Trident/4.0; SLCC2; .NET CLR 2.0.50727; .NET CLR 3.5.30729; .NET CLR 3.0.30729; Media Center PC 6.0)" },
      { "Accept-Encoding", "gzip, deflate" },
      { "Connection", "Keep-Alive" }
    }
  };

  // Create a HTTP(S) connection and send a query:
  // Reply will be available in the supplied callback function.
  //
  // Callback parameters:
  // std::shared_ptr<net::ssl>  [shared pointer to the connection object]
  // response                   [response object: contains headers and body]
  // error_code                 [the error code, if (ec) {error} else {ok}]
  //
  // make_https -> creates a TLS secured HTTPS connection (using OpenSSL)
  // make_http  -> creates a plain TCP HTTP connection
  //
  // Change 1st callback parameter to std::shared_ptr<net::tcp> for HTTP.
  make_https(ios, req.address)->query(req, [](std::shared_ptr<net::ssl>,
                                              response res,
                                              boost::system::error_code ec) {
    if (ec) {
      std::cout << "error: " << ec.message() << "\n";
    } else {
      std::cout << "HEADER:\n";
      for (auto const& header : res.headers) {
        std::cout << header.first << ": " << header.second << "\n";
      }

      std::cout << "\nCONTENT:\n";
      std::cout << "response: " << res.body << "\n";
    }
  });

  // Start asynchronous event loop.
  // This is required in order to start the request!
  ios.run();
}