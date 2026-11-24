cmake_minimum_required(VERSION 3.16)
project(certservers VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(certservers src/main.c)
target_include_directories(certservers PRIVATE include)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(certservers PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS certservers RUNTIME DESTINATION sbin)
