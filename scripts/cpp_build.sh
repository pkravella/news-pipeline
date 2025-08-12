#!/usr/bin/env bash
set -euo pipefail

# Install Conan deps & generate toolchain
conan profile detect || true
conan install . --output-folder=build/conan --build=missing -s compiler.cppstd=20

# Configure CMake with Conan toolchain
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=build/conan/conan_toolchain.cmake

# Build all targets
cmake --build build -j

echo "C++ build complete. Binaries:"
ls -la build
