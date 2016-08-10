cmake_minimum_required(VERSION 3.4.1)

set(CMAKE_VERBOSE_MAKEFILE on)

file(GLOB_RECURSE engine_SRCS ./*.c*)

include_directories(${ANDROID_NDK}/sources/android/support/include)
include_directories(${ANDROID_NDK}/sources/android/ndk_helper)

add_library(native-lib SHARED ${engine_SRCS})

target_link_libraries(native-lib log)
