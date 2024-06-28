cmake_minimum_required(VERSION 3.16)
project(net)

if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(cmake/pkg.cmake)

find_package(Threads)

option(NET_LINT "Run clang-tidy with the compiler." OFF)
if(NET_LINT)
  include(cmake/clang-tidy.cmake)
endif()

file(GLOB_RECURSE web-server-src src/web_server/*.cc)
add_library(web-server ${web-server-src} src/base64.cc)
target_compile_features(web-server PUBLIC cxx_std_23)
target_link_libraries(web-server
  ${CMAKE_THREAD_LIBS_INIT}
  boost
  boost-iostreams
  boost-url
)
target_include_directories(web-server SYSTEM PUBLIC include)
if(MSVC)
  set_target_properties(web-server PROPERTIES COMPILE_FLAGS "/bigobj")
  target_compile_definitions(web-server PUBLIC _WIN32_WINNT=0x0601)
endif()

add_library(web-server-tls ${web-server-src} src/base64.cc)
target_compile_features(web-server-tls PUBLIC cxx_std_17)
target_link_libraries(web-server-tls
  ${CMAKE_THREAD_LIBS_INIT}
  boost
  boost-iostreams
  ssl
  crypto
)
target_compile_definitions(web-server-tls PUBLIC NET_TLS=1)
target_include_directories(web-server-tls SYSTEM PUBLIC include)
if(MSVC)
  target_compile_options(web-server-tls PUBLIC "/bigobj")
  target_compile_definitions(web-server-tls PUBLIC _WIN32_WINNT=0x0601)
endif()

file(GLOB_RECURSE wss-client-src src/wss_client.cc)
add_library(wss-client ${wss-client-src})
target_compile_features(wss-client PUBLIC cxx_std_17)
target_link_libraries(wss-client
  ${CMAKE_THREAD_LIBS_INIT}
  boost
  ssl
  crypto
)
target_include_directories(wss-client PUBLIC include)
target_include_directories(wss-client SYSTEM PUBLIC include)
if(WIN32)
  target_compile_options(wss-client PUBLIC "/bigobj")
  target_compile_definitions(wss-client PUBLIC _WIN32_WINNT=0x0601)
endif()

file(GLOB_RECURSE http-client-src src/http/client/*.cc)
add_library(http-client
  ${http-client-src}
  src/ssl.cc
  src/tcp.cc
)
target_compile_features(http-client PUBLIC cxx_std_17)
target_link_libraries(http-client
  ${CMAKE_THREAD_LIBS_INIT}
  boost
  boost-iostreams
  boost-regex
  zlibstatic
  ssl
  crypto
)
target_include_directories(http-client SYSTEM PUBLIC include)
if(WIN32)
  target_compile_definitions(http-client PUBLIC _WIN32_WINNT=0x0601)
endif()

add_executable(net-web_server-sample example/web_server_example.cc)
target_link_libraries(net-web_server-sample web-server-tls)
target_compile_features(net-web_server-sample PUBLIC cxx_std_17)
target_include_directories(net-web_server-sample SYSTEM PUBLIC include ${Boost_INCLUDE_DIR})

add_executable(net-wss_client-sample example/wss_client_example.cc)
target_link_libraries(net-wss_client-sample wss-client)
target_compile_features(net-wss_client-sample PUBLIC cxx_std_17)
target_include_directories(net-wss_client-sample SYSTEM PUBLIC include ${Boost_INCLUDE_DIR})

add_executable(net-http_client-sample example/https_example.cc)
target_link_libraries(net-http_client-sample http-client)
target_compile_features(net-http_client-sample PUBLIC cxx_std_17)
target_include_directories(net-http_client-sample SYSTEM PUBLIC include ${Boost_INCLUDE_DIR})
