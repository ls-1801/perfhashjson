cmake_minimum_required(VERSION 3.27)
project(fastjson)

set(CMAKE_CXX_STANDARD 20)

find_package(simdjson REQUIRED)

add_executable(fastjson main.cpp)
target_link_libraries(fastjson simdjson)
