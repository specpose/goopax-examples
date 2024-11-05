#!/bin/bash

set -e

CMAKE_FLAGS="$CMAKE_FLAGS -DBUILD_SHARED_LIBS=0"

mkdir -p src
if [ ! -d src/eigen ]; then
    git clone https://gitlab.com/libeigen/eigen.git src/eigen -b 3.4.0
fi

cmake $CMAKE_FLAGS -B build/eigen src/eigen -DCMAKE_INSTALL_PREFIX="$PWD/dist/eigen"
cmake --build build/eigen --target install
