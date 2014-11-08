cmake_minimum_required(VERSION 2.6)
project(net)

if (MSVC)
  set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${BOOST_ROOT}\\stage\\lib")
  link_directories(${CMAKE_LIBRARY_PATH})
endif()

option(WITH_SSL "WITH_SSL" OFF)
if (WITH_SSL)
  find_package(OpenSSL REQUIRED)
  include_directories(${OPENSSL_INCLUDE_DIR})
endif()

macro(net_module name with_ssl src)
  add_library(${name} STATIC ${src})

  if(MSVC)
    set(_def
      _VARIADIC_MAX=10
      _WIN32_WINNT=0x0501
    )
    set_target_properties(${name} PROPERTIES COMPILE_DEFINITIONS "${_def}")
    set(_libs gdi32)
  elseif(MINGW)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-std=c++11")
    set(_def
      BOOST_THREAD_USE_LIB
      WINVER=0x0501
      _WIN32_WINNT=0x0501
      _WIN32_IE=0x0501
    )
    set_target_properties(${name} PROPERTIES COMPILE_DEFINITIONS "${_def}")
    set(_libs -Wl,-Bstatic gdi32 ws2_32 mswsock)
  else()
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-Wall -Wextra -std=c++11")
  endif()
  target_link_libraries(${name}
    ${_libs}
    ${ARGN}
    ${OPENSSL_LIBRARIES}
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )
endmacro(net_module)

find_package(Boost COMPONENTS system regex iostreams thread date_time)

include_directories(${Boost_INCLUDE_DIR})
include_directories("include")

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

net_module(net-tcp_client   FALSE "${TCP_CLIENT_SRC}")
net_module(net-ssl_client   TRUE  "${SSL_CLIENT_SRC}")
net_module(net-stomp        FALSE "${STOMP_SRC}" net-tcp_client)
net_module(net-base64       FALSE "${BASE64_SRC}")
net_module(net-http_server  FALSE "${HTTP_SERVER_SRC}" net-base64)
net_module(net-http_client  FALSE "${HTTP_CLIENT_SRC}" net-tcp_client)
net_module(net-https_client TRUE  "${HTTPS_CLIENT_SRC}" net-ssl_client)
net_module(net-smtp_client  TRUE  "${SMTP_SRC}" net-ssl_client)
