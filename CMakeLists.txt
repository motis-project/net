set(CMAKE_USER_MAKE_RULES_OVERRIDE
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake/c_flag_overrides.cmake)
set(CMAKE_USER_MAKE_RULES_OVERRIDE_CXX
   ${CMAKE_CURRENT_SOURCE_DIR}/cmake/cxx_flag_overrides.cmake)

cmake_minimum_required(VERSION 3.10)
project(net)

include(cmake/pkg.cmake)

if (MSVC)
  set(Boost_USE_STATIC_LIBS ON)
  set(Boost_USE_MULTITHREADED ON)
  set(Boost_USE_STATIC_RUNTIME ON)
endif()
find_package(Boost COMPONENTS system iostreams regex REQUIRED)
include_directories(SYSTEM ${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIRS})
add_definitions(/DBOOST_ALL_NO_LIB=1)

option(NET_LINT "Run clang-tidy with the compiler." OFF)
if(NET_LINT)
  # clang-tidy will be run on all targets defined hereafter
  include(cmake/clang-tidy.cmake)
endif()

file(GLOB_RECURSE web-server-src src/web_server/*.cc)
add_library(web-server ${web-server-src})
target_compile_features(web-server PUBLIC cxx_std_17)
target_link_libraries(web-server ${Boost_SYSTEM_LIBRARY} ssl crypto)
target_include_directories(web-server SYSTEM PUBLIC include ${Boost_INCLUDE_DIR})
if(MSVC)
  set_target_properties(web-server PROPERTIES COMPILE_FLAGS "${net-compile-flags} /bigobj")
endif()

file(GLOB_RECURSE wss-client-src src/wss_client.cc)
add_library(wss-client ${wss-client-src})
target_compile_features(wss-client PUBLIC cxx_std_17)
target_link_libraries(wss-client ${Boost_SYSTEM_LIBRARY} ssl crypto)
target_include_directories(wss-client PUBLIC include)

file(GLOB_RECURSE http-client-src src/http/client/*.cc)
add_library(http-client
  ${http-client-src}
  src/ssl.cc
  src/tcp.cc
)
target_compile_features(http-client PUBLIC cxx_std_17)
target_link_libraries(http-client
  ${Boost_SYSTEM_LIBRARY}
  ${Boost_IOSTREAMS_LIBRARY}
  ${Boost_REGEX_LIBRARY}
  zlib
  ssl
  crypto
)
target_include_directories(http-client PUBLIC include)

add_executable(net-web_server-sample example/web_server_example.cc)
target_link_libraries(net-web_server-sample web-server)
target_compile_features(net-web_server-sample PUBLIC cxx_std_17)

add_executable(net-wss_client-sample example/wss_client_example.cc)
target_link_libraries(net-wss_client-sample wss-client)
target_compile_features(net-wss_client-sample PUBLIC cxx_std_17)

add_executable(net-http_client-sample example/https_example.cc)
target_link_libraries(net-http_client-sample http-client)
target_compile_features(net-http_client-sample PUBLIC cxx_std_17)

add_custom_target(net-format-check
  find
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/example
    -type f
    (
    -name "*.cc"
    -o
    -name "*.h"
    )
    -print0
  | xargs -0 clang-format -i
  COMMAND
  git status --porcelain | xargs -I {} -0 test -z \"{}\"
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Checking source code formatting"
  VERBATIM
)