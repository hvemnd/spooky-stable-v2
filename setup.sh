#!/bin/sh
# Make this folder a git repository with fm1-base (and its felucca-bsp) as the submodule base/, at the commit
# Spooky Stable was built and tested against. Run once, after unzipping.
set -e
cd "$(dirname "$0")"
FM1_BASE_COMMIT=b0976f295ccaeaf77a8053a5a785a9cc21f5345f
[ -d .git ] || git init -q -b main
if [ ! -e base/tools/build.py ]; then
    rm -rf base
    git submodule add -q -f https://github.com/charlesvestal/fm1-base base   # (-f: base/ is in .gitignore for the build trees)
fi
git -C base fetch -q origin "$FM1_BASE_COMMIT" || true
git -C base checkout -q "$FM1_BASE_COMMIT"
git -C base submodule update --init --recursive -q
git add -A
git commit -q -m "Spooky Stable" || true
echo "Ready. ./build.sh --host && sh tests/run_tests.sh to test; ./build.sh for the .fwsc (see README.md)."
