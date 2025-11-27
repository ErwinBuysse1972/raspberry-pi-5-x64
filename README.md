# Raspberry Pi 5 DevContainer Cross‑Compile Environment

This repository contains a VS Code DevContainer setup for cross‑compiling **Raspberry Pi 5** userland applications and kernel modules from a powerful x86\_64 machine.

It provides:

- Automatic **sysroot syncing** from your Pi
- Preconfigured **aarch64 cross‑toolchain**
- **Userland** C/C++ build + deploy scripts
- **Kernel module** build + deploy scripts
- VS Code **IntelliSense** that understands the Pi headers & sysroot

> All commands below are intended to be run **inside the DevContainer**, unless stated otherwise.

---

## 1. Prerequisites

On your **host machine** (the PC running VS Code):

- Docker (or compatible container runtime)
- VS Code + *Dev Containers* extension
- Network access to your Raspberry Pi 5 via SSH

On your **Raspberry Pi 5**:

- 64‑bit Raspberry Pi OS (recommended)
- `ssh` server enabled
- A user with sudo rights (default: `pi`)

Take note of:

- `RPI_HOST` – hostname or IP of your Pi (e.g. `raspi5.local` or `192.168.1.42`)
- `RPI_USER` – login user on the Pi (e.g. `pi`)

---

## 2. Repository Layout

The repo is organized as follows (conventions; adjust to your own tree if needed):

```text
.
├── .devcontainer/
│   ├── devcontainer.json        # VS Code DevContainer configuration
│   └── Dockerfile               # Image with cross‑toolchain + utilities
├── cmake/
│   └── toolchain-raspi5.cmake   # CMake toolchain file for aarch64 cross‑compile
├── scripts/
│   ├── sync-sysroot.sh          # Sync sysroot from the Pi
│   ├── build-userland.sh        # Build userland binaries
│   ├── deploy-userland.sh       # Deploy userland binaries to the Pi
│   ├── build-kmod.sh            # Build kernel modules
│   └── deploy-kmod.sh           # Deploy kernel modules to the Pi
├── src/                         # Userland application code
├── kernel/                      # Out‑of‑tree kernel module(s)
└── README.md
```

The DevContainer typically mounts:

- Repository at: `/workspaces/<repo>`
- Sysroot folder at: `/opt/sysroot`

These paths are referenced in scripts and the CMake toolchain file.

---

## 3. Starting the DevContainer

1. Open this repository folder in VS Code.
2. When prompted, choose **“Reopen in Container”** (or run `Dev Containers: Reopen in Container` from the Command Palette).
3. Wait until the container is built and VS Code attaches to it.

You now have a Linux x86\_64 environment with:

- `aarch64-linux-gnu-gcc`, `aarch64-linux-gnu-g++` (or similar)
- `cmake`, `ninja` (if used)
- `rsync`, `ssh`, `scp`, etc.

---

## 4. Sysroot Syncing

To get correct headers and libraries for the Raspberry Pi, we sync the Pi’s **sysroot** into the container.

### 4.1 Environment variables

Inside the DevContainer, set (or place in `.env`, `.bashrc`, etc.):

```bash
export RPI_HOST="raspi5.local"      # or IP
export RPI_USER="pi"
export SYSROOT="/opt/sysroot"
```

### 4.2 One‑time preparation on the Pi

On the Pi (via SSH or directly), ensure the system is up to date and kernel headers are installed:

```bash
sudo apt update
sudo apt full-upgrade
sudo apt install rsync raspberrypi-kernel-headers build-essential
```

### 4.3 `scripts/sync-sysroot.sh`

Typical implementation:

```bash
#!/usr/bin/env bash
set -euo pipefail

: "${RPI_HOST:?RPI_HOST not set}"
: "${RPI_USER:?RPI_USER not set}"
: "${SYSROOT:?SYSROOT not set}"

mkdir -p "${SYSROOT}"

# Sync root filesystem but skip volatile and bulky dirs
rsync -aAXv --delete \
  --numeric-ids \
  --exclude={"/dev/*","/proc/*","/sys/*","/tmp/*","/run/*","/mnt/*","/media/*","/lost+found","/home/*"} \
  "${RPI_USER}@${RPI_HOST}:/" "${SYSROOT}"

echo "Sysroot synced to ${SYSROOT}"
```

Make it executable:

```bash
chmod +x scripts/sync-sysroot.sh
```

Run it inside the DevContainer:

```bash
scripts/sync-sysroot.sh
```

Re‑run this script whenever you update packages on the Pi and need a fresh sysroot.

---

## 5. Toolchain Setup

The DevContainer’s Dockerfile installs the cross‑compiler, e.g.:

```Dockerfile
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
        make cmake ninja-build rsync ssh && \
    rm -rf /var/lib/apt/lists/*
```

Common environment variables inside the container:

```bash
export RPI_SYSROOT="/opt/sysroot"
export RPI_TRIPLE="aarch64-linux-gnu"
export RPI_TOOLCHAIN_PREFIX="${RPI_TRIPLE}-"
```

### 5.1 `cmake/toolchain-raspi5.cmake`

Example CMake toolchain file:

```cmake
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
```

If you use 32‑bit userland on the Pi, change the triple to `arm-linux-gnueabihf` and adjust `CMAKE_SYSTEM_PROCESSOR`.

---

## 6. Userland Build + Deploy

### 6.1 Building userland code

Typical CMake‑based build from the repo root:

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-raspi5.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build -j"$(nproc)"
```

You can wrap this in `scripts/build-userland.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"

cmake -S . -B "${BUILD_DIR}" \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-raspi5.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "${BUILD_DIR}" -j"$(nproc)"
```

Run:

```bash
scripts/build-userland.sh
```

### 6.2 Deploying userland binaries

Add `scripts/deploy-userland.sh` (adjust paths/app names):

```bash
#!/usr/bin/env bash
set -euo pipefail

: "${RPI_HOST:?}"
: "${RPI_USER:?}"

BUILD_DIR="${1:-build}"
TARGET_DIR="${2:-/home/${RPI_USER}/bin}"

mkdir -p "${BUILD_DIR}"

# Example: deploy a single binary `myapp`
scp "${BUILD_DIR}/myapp" "${RPI_USER}@${RPI_HOST}:${TARGET_DIR}/"

echo "Deployed myapp to ${RPI_USER}@${RPI_HOST}:${TARGET_DIR}"
```

Example usage:

```bash
scripts/deploy-userland.sh
```

Then on the Pi:

```bash
ssh "${RPI_USER}@${RPI_HOST}" "~/bin/myapp"
```

---

## 7. Kernel Module Build + Deploy

This setup assumes **out‑of‑tree** kernel modules under `kernel/`.

### 7.1 Kernel headers in the sysroot

Because we synced the full sysroot (and installed `raspberrypi-kernel-headers` on the Pi), the kernel build tree should be available under:

```text
/opt/sysroot/lib/modules/<kernel-version>/build
```

Find the version used by your Pi:

```bash
ssh "${RPI_USER}@${RPI_HOST}" "uname -r"
```

Set in the container:

```bash
export RPI_KERNEL_VER="$(ssh "${RPI_USER}@${RPI_HOST}" "uname -r")"
export KDIR="/opt/sysroot/lib/modules/${RPI_KERNEL_VER}/build"
```

### 7.2 Module Makefile (example)

In `kernel/Makefile`:

```make
obj-m += mymodule.o

all:
	$(MAKE) -C $(KDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
```

### 7.3 `scripts/build-kmod.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

: "${KDIR:?KDIR not set}"

pushd kernel > /dev/null
make
popd > /dev/null

echo "Kernel module(s) built in kernel/"
```

Run:

```bash
export RPI_KERNEL_VER="$(ssh "${RPI_USER}@${RPI_HOST}" "uname -r")"
export KDIR="/opt/sysroot/lib/modules/${RPI_KERNEL_VER}/build"

scripts/build-kmod.sh
```

### 7.4 `scripts/deploy-kmod.sh`

```bash
#!/usr/bin/env bash
set -euo pipefail

: "${RPI_HOST:?}"
: "${RPI_USER:?}"

MODULE_NAME="${1:-mymodule}"
REMOTE_DIR="/home/${RPI_USER}/kmods"

mkdir -p kernel

scp "kernel/${MODULE_NAME}.ko" "${RPI_USER}@${RPI_HOST}:${REMOTE_DIR}/"

ssh "${RPI_USER}@${RPI_HOST}" <<EOF
set -e
cd "${REMOTE_DIR}"
sudo insmod "${MODULE_NAME}.ko" || sudo modprobe "${MODULE_NAME}" || true
sudo dmesg | tail -n 20
EOF

echo "Kernel module ${MODULE_NAME}.ko deployed and (attempted) loaded."
```

Usage:

```bash
scripts/deploy-kmod.sh mymodule
```

---

## 8. VS Code IntelliSense Setup

The DevContainer is where all tools run, so IntelliSense should be configured **inside** the container.

### 8.1 Use `compile_commands.json`

The CMake step above uses:

```bash
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

This generates `build/compile_commands.json`. To make life easier, you can:

```bash
ln -sf build/compile_commands.json .
```

Then configure VS Code to use it.

### 8.2 `.vscode/c_cpp_properties.json`

Example configuration:

```jsonc
{
    "version": 4,
    "configurations": [
        {
            "name": "Raspberry Pi 5 (aarch64)",
            "compileCommands": "${workspaceFolder}/compile_commands.json",
            "includePath": [
                "/opt/sysroot/usr/include",
                "/opt/sysroot/usr/include/aarch64-linux-gnu",
                "/opt/sysroot/include"
            ],
            "defines": [
                "__linux__",
                "__aarch64__"
            ],
            "intelliSenseMode": "linux-gcc-arm64",
            "compilerPath": "/usr/bin/aarch64-linux-gnu-g++"
        }
    ]
}
```

This tells the C/C++ extension to:

- Read includes and flags from `compile_commands.json`
- Use the **cross‑compiler** as the IntelliSense compiler
- Add extra include paths from the Raspberry Pi sysroot

### 8.3 CMake Tools (optional but recommended)

If you use the *CMake Tools* extension:

- It can **auto‑detect** the CMake toolchain file.
- It can populate the C/C++ extension automatically.

Example snippet in `.vscode/settings.json`:

```jsonc
{
    "cmake.toolchain": "cmake/toolchain-raspi5.cmake",
    "cmake.generator": "Ninja",
    "cmake.configureOnOpen": true
}
```

---

## 9. Common Pitfalls

- **“file not found” for system headers**  
  Ensure `scripts/sync-sysroot.sh` has been run and `SYSROOT` is correct.

- **Illegal instruction / wrong architecture on Pi**  
  Make sure you are using the correct triple (`aarch64-linux-gnu` for 64‑bit OS, `arm-linux-gnueabihf` for 32‑bit) and that the Pi OS matches.

- **Kernel module version mismatch**  
  The kernel headers in the sysroot must match the running kernel on the Pi (`uname -r`). If not, update the Pi and re‑sync the sysroot.

---

## 10. Cheat Sheet

```bash
# Inside DevContainer:
export RPI_HOST="raspi5.local"
export RPI_USER="pi"
export SYSROOT="/opt/sysroot"

# 1) Sync sysroot
scripts/sync-sysroot.sh

# 2) Build userland
scripts/build-userland.sh

# 3) Deploy userland binary
scripts/deploy-userland.sh

# 4) Build kernel module
export RPI_KERNEL_VER="$(ssh "${RPI_USER}@${RPI_HOST}" "uname -r")"
export KDIR="/opt/sysroot/lib/modules/${RPI_KERNEL_VER}/build"
scripts/build-kmod.sh

# 5) Deploy + load kernel module
scripts/deploy-kmod.sh mymodule
```



---

## 11. Remote Debugging from the DevContainer (gdb / VS Code)

You can remotely debug **userland binaries** running on the Raspberry Pi 5 while using the cross-debugger inside the DevContainer.

The general flow is:

1. Build your binary with debug info in the DevContainer.
2. Deploy the binary to the Pi.
3. Start `gdbserver` on the Pi, attached to your program.
4. Attach with `aarch64-linux-gnu-gdb` (or VS Code) from inside the DevContainer.

### 11.1 Prepare the Raspberry Pi

On the Pi, install `gdbserver` if not already present:

```bash
sudo apt update
sudo apt install gdbserver
```

Make sure your user can run the target binary on the Pi (e.g. it's in `~/bin` and executable).

### 11.2 Build with debug info

In the DevContainer, ensure you build with debug symbols:

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-raspi5.cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build build -j"$(nproc)"
```

Deploy the binary as usual:

```bash
scripts/deploy-userland.sh
```

Assume the binary is called `myapp` and ends up on the Pi at `~/bin/myapp`.

### 11.3 Start gdbserver on the Pi

On the Pi (SSH session or terminal):

```bash
cd ~/bin
gdbserver :1234 ./myapp
```

`gdbserver` will wait for a debugger to connect on TCP port `1234`.

### 11.4 Debug with aarch64-linux-gnu-gdb (CLI)

Inside the DevContainer:

```bash
export RPI_HOST="raspi5.local"   # or IP
export RPI_USER="pi"
export SYSROOT="/opt/sysroot"

aarch64-linux-gnu-gdb build/myapp
```

In the `gdb` prompt:

```gdb
set sysroot /opt/sysroot
set solib-search-path /opt/sysroot/lib:/opt/sysroot/usr/lib
target remote ${RPI_HOST}:1234
``

You can now set breakpoints, step through code, inspect variables, etc.

> Tip: if you use CMake with `CMAKE_SYSROOT` set (as in this repo), `set sysroot` may not be strictly necessary, but it is often helpful for correct shared library resolution in gdb.

### 11.5 Remote debugging with VS Code (C/C++ extension)

You can create a debug configuration that uses the cross-debugger inside the DevContainer and connects to `gdbserver` running on the Pi.

Create or extend `.vscode/launch.json`:

```jsonc
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Raspberry Pi 5 Remote Debug",
            "type": "cppdbg",
            "request": "launch",               // or "attach" if you prefer
            "program": "${workspaceFolder}/build/myapp",
            "args": [],
            "cwd": "${workspaceFolder}",
            "MIMode": "gdb",
            "miDebuggerPath": "/usr/bin/aarch64-linux-gnu-gdb",
            "miDebuggerServerAddress": "${env:RPI_HOST}:1234",
            "setupCommands": [
                {
                    "description": "Enable pretty-printing",
                    "text": "-enable-pretty-printing",
                    "ignoreFailures": true
                },
                {
                    "description": "Set sysroot",
                    "text": "set sysroot /opt/sysroot",
                    "ignoreFailures": true
                }
            ],
            "environment": [
                { "name": "RPI_HOST", "value": "raspi5.local" }
            ],
            "externalConsole": false
        }
    ]
}
```

Usage:

1. On the Pi: run `gdbserver :1234 ~/bin/myapp`.
2. In VS Code (inside the DevContainer): select **“Raspberry Pi 5 Remote Debug”** and start debugging (F5).

VS Code will:

- Use `aarch64-linux-gnu-gdb` inside the container.
- Connect to `gdbserver` on the Pi via `miDebuggerServerAddress`.
- Use the local `build/myapp` binary (with symbols) and the synced sysroot for correct source + library mapping.

### 11.6 Optional: preLaunchTask to deploy & start gdbserver

You can automate the deploy + start-gdbserver step with a VS Code task.

Example `.vscode/tasks.json` snippet:

```jsonc
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "deploy-myapp",
            "type": "shell",
            "command": "scripts/deploy-userland.sh && ssh ${RPI_USER}@${RPI_HOST} 'cd ~/bin && gdbserver :1234 ./myapp'"
        }
    ]
}
```

Then in `launch.json`, reference it:

```jsonc
"preLaunchTask": "deploy-myapp"
```

> Note: because `gdbserver` will take over the SSH session, this simple approach is best when run in an integrated terminal where you can see its output. For more advanced setups, you can use systemd services, tmux, or custom scripts on the Pi to manage `gdbserver`.

---

With this in place, the DevContainer is not just a cross-compile environment but also a **full remote debug workstation** for your Raspberry Pi 5 userland applications.


You can customize scripts, directories, and toolchain details to match your specific Raspberry Pi 5 project layout. This README documents the default conventions used in this DevContainer setup.
