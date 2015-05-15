cmake_minimum_required(VERSION 2.6)
project(net)

if (MSVC)
  set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${BOOST_ROOT}\\stage\\lib")
  link_directories(${CMAKE_LIBRARY_PATH})
endif()

option(NO_ZLIB "NO_ZLIB" OFF)
option(NO_SSL "NO_SSL" OFF)

function(net_module name with_ssl with_zlib src)
  if (NO_SSL AND with_ssl)
    return()
  endif()

  add_library(${name} STATIC ${src})

  if (with_ssl)
    set(_all_dev WITH_SSL=true)
    if (NOT OPENSSL_FOUND)
      find_package(OpenSSL REQUIRED)
      include_directories(${OPENSSL_INCLUDE_DIR})
    endif()
  endif()

  if (with_zlib AND NOT NO_ZLIB)
    set(_all_dev ${_all_dev} WITH_ZLIB=true)
    if (NOT ZLIB_FOUND)
      find_package(ZLIB REQUIRED)
      include_directories(${ZLIB_INCLUDE_DIRS})
    endif()
  endif()

  if(MSVC)
    set(_def
      ${_all_dev}
      _VARIADIC_MAX=10
      _WIN32_WINNT=0x0501
    )
    set_target_properties(${name} PROPERTIES COMPILE_DEFINITIONS "${_def}")
    set(_libs gdi32)
  elseif(MINGW)
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-std=c++11")
    set(_def
      ${_all_dev}
      BOOST_THREAD_USE_LIB
      WINVER=0x0501
      _WIN32_WINNT=0x0501
      _WIN32_IE=0x0501
    )
    set_target_properties(${name} PROPERTIES COMPILE_DEFINITIONS "${_def}")
    set(_libs -Wl,-Bstatic gdi32 ws2_32 mswsock)
  else()
    set(_def
      ${_all_dev}
    )
    set_target_properties(${name} PROPERTIES COMPILE_DEFINITIONS "${_def}")
    set_target_properties(${name} PROPERTIES COMPILE_FLAGS "-Wall -Wextra -std=c++11")
  endif()

  target_link_libraries(${name}
    ${_libs}
    ${ARGN}
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
  )

  unset(_all_dev)
endfunction(net_module)

find_package(Boost COMPONENTS system regex iostreams thread date_time)

include_directories(${Boost_INCLUDE_DIR})
include_directories("include")

file(GLOB_RECURSE STOMP_SRC        "src/stomp_client.cc")
file(GLOB_RECURSE BASE64_SRC       "src/base64.cc")
file(GLOB_RECURSE HTTP_SERVER_SRC  "src/http/server/*.cc")
file(GLOB_RECURSE TCP_CLIENT_SRC   "src/tcp.cc")
file(GLOB_RECURSE HTTP_CLIENT_SRC  "src/http/client/*.cc")
file(GLOB_RECURSE SMTP_SRC         "src/smtp.cc")
file(GLOB_RECURSE SSL_CLIENT_SRC   "src/ssl.cc")

net_module(net-tcp_client   FALSE FALSE "${TCP_CLIENT_SRC}")
net_module(net-ssl_client   TRUE  FALSE "${SSL_CLIENT_SRC}")
net_module(net-stomp        FALSE FALSE "${STOMP_SRC}" net-tcp_client)
net_module(net-base64       FALSE FALSE "${BASE64_SRC}")
net_module(net-http_server  FALSE FALSE "${HTTP_SERVER_SRC}" net-base64)
net_module(net-http_client  FALSE TRUE  "${HTTP_CLIENT_SRC}" net-tcp_client)
net_module(net-https_client TRUE  TRUE  "${HTTP_CLIENT_SRC}" net-ssl_client)
net_module(net-smtp_client  TRUE  FALSE "${SMTP_SRC}" net-ssl_client)