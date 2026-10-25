#!/bin/sh
# From a computer: copy the built package to the MPC and run the installer.   ./scripts/deploy.sh <mpc-ip>
set -e
[ -n "$1" ] || { echo "usage: $0 <mpc-ip>"; exit 1; }
cd "$(dirname "$0")/.."
scp -r dist/RSFX-1.1.0 "root@$1:/tmp/"
ssh -t "root@$1" sh /tmp/RSFX-1.1.0/install.sh
