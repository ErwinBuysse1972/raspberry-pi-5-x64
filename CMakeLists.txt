cmake_minimum_required(VERSION 3.22)

project(rpi5_dev LANGUAGES C CXX)

# --------------------------------------------------------------------
# Global C++ settings (C++20 for cliApplication & tracing)
# --------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --------------------------------------------------------------------
# Subdirectories
# --------------------------------------------------------------------
add_subdirectory(userland/hello_pi)
add_subdirectory(userland/cli-application)
add_subdirectory(userland/cli-application/Tracer)

# --------------------------------------------------------------------
# Raspberry Pi connection / paths
# --------------------------------------------------------------------
set(RPI_USER "erwinbuysse" CACHE STRING "Raspberry Pi SSH username")
set(RPI_HOST "192.168.0.127" CACHE STRING "Raspberry Pi hostname or IP")

# Absolute path for the deploy dir
set(RPI_REMOTE_DIR "/home/${RPI_USER}/bin"
    CACHE STRING "Remote directory on the Raspberry Pi")

set(RPI_GDB_PORT "2345" CACHE STRING "TCP port used by gdbserver")
set(RPI_GDBSERVER_PATH "/usr/bin/gdbserver"
    CACHE STRING "Path to gdbserver on the Raspberry Pi")

# Explicit ssh/scp paths in the devcontainer
set(SSH_PATH "/usr/bin/ssh"
    CACHE STRING "Path to ssh on the dev host")
set(SCP_PATH "/usr/bin/scp"
    CACHE STRING "Path to scp on the dev host")

# --------------------------------------------------------------------
# Deploy targets for cliApplication
#   - used by your CMakePresets:
#       rpi-userland-debug-deploy  -> deploy_debug
#       rpi-userland-release-deploy -> deploy_release
#       rpi-userland-debug-remote-debug -> deploy_debug_gdb
# --------------------------------------------------------------------

# DEBUG deploy (build + copy to cliApplication_debug via .new + mv)
add_custom_target(deploy_debug
    DEPENDS cliApplication tracing

    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST} mkdir -p ${RPI_REMOTE_DIR}

    # Copy executable
    COMMAND ${SCP_PATH} "$<TARGET_FILE:cliApplication>"
                        ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/cliApplication_debug.new

    # Copy tracing library
    COMMAND ${SCP_PATH} "$<TARGET_FILE:tracing>"
                        ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/libtracing.so.new

    # Atomically replace
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            mv ${RPI_REMOTE_DIR}/cliApplication_debug.new ${RPI_REMOTE_DIR}/cliApplication_debug

    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            mv ${RPI_REMOTE_DIR}/libtracing.so.new ${RPI_REMOTE_DIR}/libtracing.so

    COMMENT "Deploying DEBUG cliApplication + libtracing.so"
)


# RELEASE deploy (build + copy to cliApplication via .new + mv)
add_custom_target(deploy_release
    DEPENDS cliApplication

    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST} mkdir -p ${RPI_REMOTE_DIR}

    COMMAND ${SCP_PATH} "$<TARGET_FILE:cliApplication>"
                        ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/cliApplication.new

    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            mv ${RPI_REMOTE_DIR}/cliApplication.new ${RPI_REMOTE_DIR}/cliApplication

    COMMENT "Deploying RELEASE build of cliApplication to ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/cliApplication"
)

# DEBUG deploy + start gdbserver (for remote debugging)
add_custom_target(deploy_debug_gdb
    DEPENDS cliApplication tracing

    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST} mkdir -p ${RPI_REMOTE_DIR}

    # Copy files
    COMMAND ${SCP_PATH} "$<TARGET_FILE:cliApplication>"
                        ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/cliApplication_debug.new
    COMMAND ${SCP_PATH} "$<TARGET_FILE:tracing>"
                        ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/libtracing.so.new

    # Move into place
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            mv ${RPI_REMOTE_DIR}/cliApplication_debug.new ${RPI_REMOTE_DIR}/cliApplication_debug
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            mv ${RPI_REMOTE_DIR}/libtracing.so.new ${RPI_REMOTE_DIR}/libtracing.so

    # 🔥 Start gdbserver with cliApplication_debug instead of hello_pi
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            "${RPI_GDBSERVER_PATH} :${RPI_GDB_PORT} ${RPI_REMOTE_DIR}/cliApplication_debug"

    COMMENT "Deploying DEBUG cliApplication + libtracing.so and starting gdbserver"
)