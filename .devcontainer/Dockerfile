# Base image with vscode user and devcontainer niceties
FROM mcr.microsoft.com/devcontainers/base:ubuntu-24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential \
        cmake ninja-build \
        git pkg-config \
        gdb-multiarch \
        gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
        crossbuild-essential-arm64 \
        rsync openssh-client \
        # Kernel build deps
        bc flex bison \
        libssl-dev \
        libncurses-dev \
        dwarves \
        cpio kmod \
    && rm -rf /var/lib/apt/lists/*

# Environment for cross-compiling to Pi 5
ENV ARCH=arm64
ENV CROSS_COMPILE=aarch64-linux-gnu-
ENV RPI_SYSROOT=/opt/rpi-sysroot
ENV RPI_KERNEL=/opt/rpi-kernel
