cmake_minimum_required(VERSION 3.14)

project(tracing LANGUAGES CXX)

add_library(tracing SHARED
    cfunctracer.cpp
    ctracer.cpp
    cscopedtimer.cpp
)

target_include_directories(tracing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(tracing
    PRIVATE
        TRACING_LIBRARY
)
