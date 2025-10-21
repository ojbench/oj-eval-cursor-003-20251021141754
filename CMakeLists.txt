cmake_minimum_required(VERSION 3.10)
project(icpc_mgmt_system)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(code src/main.cpp)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(code PRIVATE -O3 -pipe -s -DNDEBUG)
endif()
