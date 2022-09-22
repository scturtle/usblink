cmake_minimum_required(VERSION 3.18.4)
# or use: /opt/devkitpro/portlibs/switch/bin/aarch64-none-elf-cmake
include(/opt/devkitpro/cmake/Switch.cmake)
project(usblink VERSION 0.0.1 LANGUAGES C CXX)
set(PROJECT_AUTHOR "scturtle")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
endif()

set(TARGET ${PROJECT_NAME})

add_executable(${TARGET} main.cc usb.cc)
target_compile_features(${TARGET} PUBLIC cxx_std_17)

nx_generate_nacp(${TARGET}.nacp
  NAME ${TARGET}
  AUTHOR ${PROJECT_AUTHOR}
  )
message(${CMAKE_SOURCE_DIR}/icon.png)
nx_create_nro(${TARGET}
  NACP ${TARGET}.nacp
  ICON ${CMAKE_SOURCE_DIR}/icon.jpg
  )
