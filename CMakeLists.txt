cmake_minimum_required(VERSION 3.22)
project(rpi5_dev C)

# --------------------------------------------------------------------
# Target
# --------------------------------------------------------------------
add_executable(hello_pi
    userland/test/main.c
)

# --------------------------------------------------------------------
# Raspberry Pi connection / paths
# --------------------------------------------------------------------
set(RPI_USER "erwinbuysse" CACHE STRING "Raspberry Pi SSH username")
set(RPI_HOST "192.168.0.127" CACHE STRING "Raspberry Pi hostname or IP")

# Absolute path for the deploy dir
set(RPI_REMOTE_DIR "/home/${RPI_USER}/bin" CACHE STRING "Remote directory for hello_pi on the Raspberry Pi")

set(RPI_GDB_PORT "2345" CACHE STRING "TCP port used by gdbserver")
set(RPI_GDBSERVER_PATH "/usr/bin/gdbserver" CACHE STRING "Path to gdbserver on the Raspberry Pi")

# Explicit ssh/scp paths in the devcontainer
set(SSH_PATH "/usr/bin/ssh" CACHE STRING "Path to ssh on the dev host")
set(SCP_PATH "/usr/bin/scp" CACHE STRING "Path to scp on the dev host")

# --------------------------------------------------------------------
# Deploy targets
# --------------------------------------------------------------------

# DEBUG deploy (build + copy to hello_pi_debug via .new + mv)
# -> preset: rpi-userland-debug-deploy    (targets: ["deploy_debug"])
add_custom_target(deploy_debug
    DEPENDS hello_pi

    # 1) Ensure remote directory exists
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST} mkdir -p ${RPI_REMOTE_DIR}

    # 2) Copy to a temporary .new file
    COMMAND ${SCP_PATH} "$<TARGET_FILE:hello_pi>"
                        ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/hello_pi_debug.new

    # 3) Atomically replace the old binary
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            mv ${RPI_REMOTE_DIR}/hello_pi_debug.new ${RPI_REMOTE_DIR}/hello_pi_debug

    COMMENT "Deploying DEBUG build of hello_pi to ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/hello_pi_debug"
)

# RELEASE deploy (build + copy to hello_pi via .new + mv)
# -> preset: rpi-userland-release-deploy  (targets: ["deploy_release"])
add_custom_target(deploy_release
    DEPENDS hello_pi

    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST} mkdir -p ${RPI_REMOTE_DIR}

    COMMAND ${SCP_PATH} "$<TARGET_FILE:hello_pi>"
                        ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/hello_pi.new

    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            mv ${RPI_REMOTE_DIR}/hello_pi.new ${RPI_REMOTE_DIR}/hello_pi

    COMMENT "Deploying RELEASE build of hello_pi to ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/hello_pi"
)

# DEBUG deploy + start gdbserver (for remote debugging)
# -> preset: rpi-userland-debug-remote-debug  (targets: ["deploy_debug_gdb"])
# DEBUG deploy + start gdbserver (for remote debugging)
# -> preset: rpi-userland-debug-remote-debug  (targets: ["deploy_debug_gdb"])
add_custom_target(deploy_debug_gdb
    DEPENDS hello_pi

    # 1) Ensure remote directory exists
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST} mkdir -p ${RPI_REMOTE_DIR}

    # 2) Copy to temporary .new file
    COMMAND ${SCP_PATH} "$<TARGET_FILE:hello_pi>"
                        ${RPI_USER}@${RPI_HOST}:${RPI_REMOTE_DIR}/hello_pi_debug.new

    # 3) Atomically replace the old binary
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            mv ${RPI_REMOTE_DIR}/hello_pi_debug.new ${RPI_REMOTE_DIR}/hello_pi_debug

    # 4) Start gdbserver with the new binary (absolute path, no cd/&&)
    COMMAND ${SSH_PATH} ${RPI_USER}@${RPI_HOST}
            "${RPI_GDBSERVER_PATH} :${RPI_GDB_PORT} ${RPI_REMOTE_DIR}/hello_pi_debug"

    COMMENT "Deploying DEBUG build of hello_pi and starting gdbserver on ${RPI_USER}@${RPI_HOST}:${RPI_GDB_PORT}"
)