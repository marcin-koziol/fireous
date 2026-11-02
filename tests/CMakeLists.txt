cmake_minimum_required(VERSION 3.15)
project(fireous-tests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

enable_testing()

add_executable(test_engine test_engine.cpp)
target_include_directories(test_engine PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../plugins/FIReous")
target_compile_options(test_engine PRIVATE -Wall -Wextra)

add_test(NAME engine COMMAND test_engine)
