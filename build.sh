#!/usr/bin/env bash
set -e

BUILD_DIR="build"
BUILD_TYPE="${1:-Debug}"

cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 4)"

echo ""
echo "Build complete: $BUILD_DIR/szip"
