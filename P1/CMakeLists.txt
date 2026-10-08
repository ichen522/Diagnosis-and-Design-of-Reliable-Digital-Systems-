cmake_minimum_required(VERSION 3.10)
project(EE658_P1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(simulator
  main.cpp
  readckt.cpp
  levelizer.cpp
  simulator.cpp
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(simulator PRIVATE -Wall -Wextra)
endif()
