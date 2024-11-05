#!/bin/bash

set -e

export CMAKE_FLAGS="-DCMAKE_BUILD_TYPE=Release $CMAKE_FLAGS"

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
