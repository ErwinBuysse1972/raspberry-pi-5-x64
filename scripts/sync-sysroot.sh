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