cmake_minimum_required(VERSION 2.6)
project(net)

macro(net_module name src)
  add_library(${name} STATIC ${src})

  if(MSVC)
    target_link_libraries(${name} ${Boost_LIBRARIES} ${ARGN} gdi32)
    set(_def
      _VARIADIC_MAX=10
      _WIN32_WINNT=0x0501
    )
    set_target_properties(${name} PROPERTIES COMPILE_DEFINITIONS "${_def}")
  elseif(MINGW)
    target_link_libraries(${name} -Wl,-Bstatic ${Boost_LIBRARIES} ${ARGN} z gdi32 ws2_32 mswsock)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-std=c++11")
    set(_def
      BOOST_THREAD_USE_LIB
      WINVER=0x0501
      _WIN32_WINNT=0x0501
      _WIN32_IE=0x0501
    )
    set_target_properties(${name} PROPERTIES COMPILE_DEFINITIONS "${_def}")
  else()
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-Wall -Wextra -std=c++11")
    target_link_libraries(${name} ${Boost_LIBRARIES} z ${ARGN} ssl crypto ${CMAKE_THREAD_LIBS_INIT})
  endif()
endmacro(net_module)

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
file(GLOB_RECURSE BASE64_SRC              "src/base64.cc")
file(GLOB_RECURSE HTTP_SERVER_SRC         "src/http/server/*.cc")
file(GLOB_RECURSE TCP_CLIENT_SRC          "src/tcp.cc")
file(GLOB_RECURSE BASIC_HTTP_CLIENT_SRC   "src/http/client/*.cc")
list(REMOVE_ITEM  BASIC_HTTP_CLIENT_SRC   "src/http/client/https_client.cc")
list(REMOVE_ITEM  BASIC_HTTP_CLIENT_SRC   "src/http/client/http_client.cc")
file(GLOB_RECURSE HTTP_CLIENT_SRC         "src/http/client/http_client.cc" ${BASIC_HTTP_CLIENT_SRC})
file(GLOB_RECURSE HTTPS_CLIENT_SRC        "src/http/client/https_client.cc" ${BASIC_HTTP_CLIENT_SRC})
file(GLOB_RECURSE SMTP_SRC                "src/smtp.cc")
file(GLOB_RECURSE SSL_CLIENT_SRC          "src/ssl.cc")

net_module(net-tcp_client   ${TCP_CLIENT_SRC})
net_module(net-ssl_client   ${SSL_CLIENT_SRC})
net_module(net-stomp        ${STOMP_SRC} net-tcp_client)
net_module(net-base64       ${BASE64_SRC})
net_module(net-http_server  ${HTTP_SERVER_SRC} net-base64)
net_module(net-http_client  ${HTTP_CLIENT_SRC} net-tcp_client)
net_module(net-https_client ${HTTPS_CLIENT_SRC} net-ssl_client)
net_module(net-smtp_client  ${SMTP_SRC} net-ssl_client)
