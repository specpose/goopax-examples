#!/bin/bash

set -e

CMAKE_FLAGS="$CMAKE_FLAGS -DBUILD_SHARED_LIBS=0"

mkdir -p src
if [ ! -d src/boost ]; then
    git clone https://github.com/boostorg/boost.git src/boost -b boost-1.86.0 --recurse-submodules
fi

cmake $CMAKE_FLAGS -B build/boost src/boost -DCMAKE_INSTALL_PREFIX="$PWD/dist/boost"
cmake --build build/boost --target install
