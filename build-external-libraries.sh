#!/bin/bash

export CMAKE_FLAGS="$CMAKE_FLAGS"
export CXXFLAGS="$CXXFLAGS"

set -e
set -o nounset
set -o pipefail

#export CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL"

cd ext
./build-eigen.sh
./build-sdl.sh
./build-opencv.sh

if  [ "$(uname -o)" == "Msys" ]; then
    echo "Not building boost and gmp on windows due to some difficulties"
else
    ./build-boost.sh
    ./build-gmp.sh
fi
