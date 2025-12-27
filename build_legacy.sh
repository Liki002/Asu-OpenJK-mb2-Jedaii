#!/bin/bash
set -e

echo "=== Building OpenJK for Legacy Linux (GLIBC 2.31) ==="
echo "This requires Docker. You may be asked for your sudo password."

# 1. Build the Docker image
echo ">>> Building Docker image..."
docker build -f Dockerfile.mb2 -t openjk_legacy_builder .
id=$(docker create openjk_legacy_builder)
docker cp $id:/openjk_build/OpenJK/build/openjkded.i386 ./build-server/openjkded.i386
docker rm -v $id

echo "=== Success! ==="
echo "The compatible binary is at: build-server/openjkded.i386"
