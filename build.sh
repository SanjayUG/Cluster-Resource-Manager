#!/bin/bash

set -e

echo "[BUILD] Cleaning old build files..."
make clean

echo "[BUILD] Building project..."
make

echo "[BUILD] Build complete. Binaries are in the build/ directory." 