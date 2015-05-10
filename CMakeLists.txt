project(nodecxx)
cmake_minimum_required(VERSION 3.0)

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++1z")

find_package(Boost REQUIRED COMPONENTS system)
include_directories(${Boost_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR})

set(SRCS
    main.cpp
    core.cpp
    http/http_parser.h
    http/http_parser.c
    http/http.hpp
    http/http.cpp)
add_executable(node ${SRCS})
target_link_libraries(node ${Boost_LIBRARIES})
