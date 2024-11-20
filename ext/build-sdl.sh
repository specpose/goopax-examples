#!/bin/bash

set -e

CMAKE_FLAGS="$CMAKE_FLAGS -DBUILD_SHARED_LIBS=0"

if [ ! -d src/sdl3 ]; then
    git clone https://github.com/libsdl-org/SDL.git src/sdl3
fi

cmake $CMAKE_FLAGS -B build/sdl3 src/sdl3 -DSDL_SHARED=0 -DSDL_STATIC=1 -DSDL_ASSEMBLY=0 -DCMAKE_INSTALL_PREFIX="$PWD/dist/sdl3"
cmake --build build/sdl3 --target install
