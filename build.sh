#!/bin/sh
# Build Spooky Stable. Needs fm1-base in base/ (./setup.sh), Python 3 with Pillow, and for the FM-1 package
# JieLi's toolchain ($JIELI_TOOLCHAIN) and the AC79 SDK ($AC79_SDK); on macOS the toolchain runs in Docker.
#   ./build.sh                  app/build/spooky.fwsc
#   ./build.sh --release 1.0    app/build/spooky-1.0.fwsc
#   ./build.sh --host           the host simulator (no JieLi tools needed): app/build/host/spooky_host
set -e
cd "$(dirname "$0")"
exec python3 base/tools/build.py app "$@"
