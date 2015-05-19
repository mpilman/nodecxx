project(nodecxx)
cmake_minimum_required(VERSION 3.0)

option(USE_ASAN "Use address sanitizer" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")
if (USE_ASAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-omit-frame-pointer -fno-optimize-sibling-calls -fsanitize=address")
endif()

find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR})
include_directories("${CMAKE_SOURCE_DIR}/externals/uriparser/include")

set(SRCS
    main.cpp
    core.cpp
    net/net.hpp
    net/net.cpp
    http/http_parser.h
    http/http_parser.c
    http/http.hpp
    http/http.cpp
    http/url.hpp
    http/url.cpp
    fs/fs.hpp
    fs/fs.cpp
    externals/json11/json11.cpp
    express/express.hpp
    express/express.cpp
    externals/uriparser/src/UriCommon.h
    externals/uriparser/src/UriCommon.c
    externals/uriparser/src/UriCompare.c
    externals/uriparser/src/UriEscape.c
    externals/uriparser/src/UriFile.c
    externals/uriparser/src/UriIp4.c
    externals/uriparser/src/UriIp4Base.c
    externals/uriparser/src/UriIp4Base.h
    externals/uriparser/src/UriNormalize.c
    externals/uriparser/src/UriNormalizeBase.c
    externals/uriparser/src/UriNormalizeBase.h
    externals/uriparser/src/UriParse.c
    externals/uriparser/src/UriParseBase.c
    externals/uriparser/src/UriParseBase.h
    externals/uriparser/src/UriQuery.c
    externals/uriparser/src/UriRecompose.c
    externals/uriparser/src/UriResolve.c
    externals/uriparser/src/UriShorten.c
    )
add_executable(node ${SRCS})
target_link_libraries(node ${Boost_LIBRARIES})
