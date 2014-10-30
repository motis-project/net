cmake_minimum_required(VERSION 2.6)
project(net)

find_package(Boost COMPONENTS
  system
  regex
  iostreams
  thread
  date_time
REQUIRED)

include_directories(${Boost_INCLUDE_DIR})
include_directories("./include")

file(GLOB_RECURSE STOMP_SRC               "src/stomp_client.cc")
file(GLOB_RECURSE BASE64_SRC              "src/net/base64.cc")
file(GLOB_RECURSE HTTP_SERVER_SRC         "src/http/server/*.cc")
file(GLOB_RECURSE TCP_CLIENT_SRC          "src/tcp.cc")
file(GLOB_RECURSE BASIC_HTTP_CLIENT_SRC   "src/http/client/*.cc")
list(REMOVE_ITEM  BASIC_HTTP_CLIENT_SRC   "src/http/client/https_client.cc")
list(REMOVE_ITEM  BASIC_HTTP_CLIENT_SRC   "src/http/client/http_client.cc")
file(GLOB_RECURSE HTTP_CLIENT_SRC         "src/http/client/http_client.cc")
file(GLOB_RECURSE HTTPS_CLIENT_SRC        "src/http/client/https_client.cc")
file(GLOB_RECURSE SMTP_SRC                "src/smtp.cc")
file(GLOB_RECURSE SSL_CLIENT_SRC          "src/ssl.cc")

add_library(net STATIC ${NET_SRC})

if(MSVC)
  target_link_libraries(net ssl crypto gdi32 ${Boost_LIBRARIES})
  set(_def
    _VARIADIC_MAX=10
    _WIN32_WINNT=0x0501
  )
  set_target_properties(net PROPERTIES COMPILE_DEFINITIONS "${_def}")
elseif(MINGW)
  target_link_libraries(net -Wl,-Bstatic ${Boost_LIBRARIES} ssl crypto z gdi32 ws2_32 mswsock)
  set_target_properties(net PROPERTIES COMPILE_FLAGS "-std=c++11")
  set(_def
    BOOST_THREAD_USE_LIB
    WINVER=0x0501
    _WIN32_WINNT=0x0501
    _WIN32_IE=0x0501
  )
  set_target_properties(net PROPERTIES COMPILE_DEFINITIONS "${_def}")
else()
  set_target_properties(net PROPERTIES COMPILE_FLAGS "-Wall -Wextra -std=c++11")
  if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(net ${Boost_LIBRARIES} z ssl crypto)
  else()
    target_link_libraries(net ${Boost_LIBRARIES} z ssl crypto dl -pthread)
  endif()
endif()
