cmake_minimum_required(VERSION 3.15)
project(fireous)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/dpf/distrho/DistrhoPlugin.hpp")
  message(FATAL_ERROR "dpf/ submodule not found - run: git submodule update --init --recursive")
endif()

add_subdirectory(dpf)
add_subdirectory(plugins/FIReous)
