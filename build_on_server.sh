#!/bin/bash
set -e

echo "=== OpenJK Build Helper ==="
echo "This script will install dependencies and build the i386 dedicated server."

# 1. Install Dependencies
echo ">>> Installing dependencies... (SKIPPED to avoid sudo)"
# if command -v apt-get &> /dev/null; then
#     sudo dpkg --add-architecture i386
#     sudo apt-get update
#     sudo apt-get install -y build-essential cmake gcc-multilib g++-multilib \
#                          libjpeg-dev:i386 libpng-dev:i386 zlib1g-dev:i386 git
# else
#     echo "Warning: apt-get not found. Ensure required dependencies are installed manually."
# fi

# 2. Setup Build Directory
echo ">>> Setting up build directory..."
rm -rf build-server
mkdir build-server
cd build-server

# 3. Configure (CMake)
echo ">>> Configuring CMake..."
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/Toolchains/linux-i686.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DBuildMPCGame=ON \
      -DBuildMPGame=ON \
      -DBuildMPEngine=OFF \
      -DBuildMPRdVanilla=OFF \
      -DBuildMPUI=OFF \
      -DBuildSPEngine=OFF \
      -DBuildSPGame=OFF \
      -DBuildSPRdVanilla=OFF \
      -DBuildMPRend2=OFF \
      ..

# 4. Build
echo ">>> Building..."
make -j$(nproc)

echo "=== Build Complete ==="
echo "Binaries located at:"
echo "Engine:   $(pwd)/openjkded.i386"
echo "Game DLL: $(pwd)/codemp/jampgamei386.so"
echo "Copy THESE binaries to your server's base directory/MBII folder."
