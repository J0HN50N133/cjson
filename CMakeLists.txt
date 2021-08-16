cmake_minimum_required(VERSION 3.16)
project(cjson C)

set(CMAKE_C_STANDARD 99)

include_directories(.)
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ansi -pedantic -Wall")
endif()
add_library(cjson cjson.c)
add_executable(cjson_test cjson_test.c)
target_link_libraries(cjson_test cjson)