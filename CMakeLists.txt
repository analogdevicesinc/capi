# CAPI — Common API interface library
#
# Provides: capi_core OBJECT library with dispatch sources + public headers
# Usage: add_subdirectory(<capi-path>) then target_link_libraries(app capi::core)

cmake_minimum_required(VERSION 3.21)

file(GLOB CAPI_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_library(capi_core OBJECT ${CAPI_SOURCES})

target_include_directories(capi_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/inc
)

add_library(capi::core ALIAS capi_core)
