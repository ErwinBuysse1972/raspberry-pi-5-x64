# Target system
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross compiler
set(RPI_TRIPLE aarch64-linux-gnu)
set(CMAKE_C_COMPILER   "${RPI_TRIPLE}-gcc")
set(CMAKE_CXX_COMPILER "${RPI_TRIPLE}-g++")

# Sysroot (mounted in container)
set(RPI_SYSROOT "/opt/sysroot")
set(CMAKE_SYSROOT "${RPI_SYSROOT}")

# Use sysroot for search paths
set(CMAKE_FIND_ROOT_PATH "${RPI_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Basic flags (adjust as needed)
set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS}   --sysroot=${RPI_SYSROOT}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --sysroot=${RPI_SYSROOT}")