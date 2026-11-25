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

enable_testing()
add_test(NAME scaffold_smoke COMMAND certservers)
set_tests_properties(scaffold_smoke PROPERTIES
    PASS_REGULAR_EXPRESSION "certservers scaffold: implementation pending"
)

install(TARGETS certservers RUNTIME DESTINATION sbin)

option(CERTSERVERS_WITH_TONGSUO "Build Tongsuo 8.5.0 integration smoke test" OFF)
set(TONGSUO_ROOT "" CACHE PATH "Absolute Tongsuo installation prefix")

if(CERTSERVERS_WITH_TONGSUO)
    if(NOT TONGSUO_ROOT)
        message(FATAL_ERROR "CERTSERVERS_WITH_TONGSUO requires -DTONGSUO_ROOT=/absolute/prefix")
    endif()

    find_path(TONGSUO_INCLUDE_DIR openssl/tsapi.h
        PATHS "${TONGSUO_ROOT}/include"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(TONGSUO_SSL_LIBRARY ssl
        PATHS "${TONGSUO_ROOT}/lib" "${TONGSUO_ROOT}/lib64"
        NO_DEFAULT_PATH
        REQUIRED
    )
    find_library(TONGSUO_CRYPTO_LIBRARY crypto
        PATHS "${TONGSUO_ROOT}/lib" "${TONGSUO_ROOT}/lib64"
        NO_DEFAULT_PATH
        REQUIRED
    )

    add_executable(tongsuo_smoke tests/tongsuo_smoke.c)
    target_include_directories(tongsuo_smoke PRIVATE "${TONGSUO_INCLUDE_DIR}")
    target_link_libraries(tongsuo_smoke PRIVATE
        "${TONGSUO_SSL_LIBRARY}"
        "${TONGSUO_CRYPTO_LIBRARY}"
    )
    set_target_properties(tongsuo_smoke PROPERTIES
        BUILD_RPATH "${TONGSUO_ROOT}/lib;${TONGSUO_ROOT}/lib64"
    )
    add_test(NAME tongsuo_8_5_tsapi_smoke COMMAND tongsuo_smoke)
endif()
