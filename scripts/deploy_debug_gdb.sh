#!/usr/bin/env bash
set -euo pipefail

BIN="$1"        # full path to hello_pi on the devcontainer
USER="$2"       # RPI_USER
HOST="$3"       # RPI_HOST
PORT="$4"       # RPI_GDB_PORT
REMOTE_DIR="$5" # RPI_REMOTE_DIR, e.g. ~/bin

echo "Deploying $BIN to $USER@$HOST:$REMOTE_DIR/hello_pi_debug and starting gdbserver on port $PORT"

ssh "${USER}@${HOST}" "mkdir -p ${REMOTE_DIR}"
scp "${BIN}" "${USER}@${HOST}:${REMOTE_DIR}/hello_pi_debug"
ssh "${USER}@${HOST}" "cd ${REMOTE_DIR} && /usr/bin/gdbserver :${PORT} ./hello_pi_debug"
