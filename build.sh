#!/bin/bash

set -e

export goopax_DIR="$PWD/.."
export SDL2_DIR=ext/dist/sdl2
export Eigen3_DIR=ext/dist/eigen
export OpenCV_DIR=ext/dist/opencv
export Boost_DIR=ext/dist/boost
export CMAKE_PREFIX_PATH="$PWD/ext/dist/gmp"

if  [ "$(uname -o)" == "Msys" ]; then
	CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_CONFIGURATION_TYPES=Debug -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug" -DOpenCV_STATIC=ON
fi

cmake $CMAKE_FLAGS -B build src -DCMAKE_INSTALL_PREFIX="$PWD/dist"

if (echo "$CMAKE_FLAGS" | grep CMAKE_SYSTEM_NAME=iOS); then
    echo "iOS. Skipping install. Please open build/goopax_examples.xcodeproj and build manually."
else
    cmake --build build --target install
fi
