cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB_RECURSE engine_SRCS ./*.c*)

add_library(stockfish-lib SHARED ${engine_SRCS})

target_link_libraries(stockfish-lib log)
