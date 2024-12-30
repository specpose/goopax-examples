#!/bin/bash

set -e

if  [ "$(uname -o)" == "Msys" ]; then
	CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_CONFIGURATION_TYPES=Debug -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug" -DOpenCV_STATIC=ON
fi

cmake $CMAKE_FLAGS -B build src -DCMAKE_INSTALL_PREFIX="$PWD/dist"
cmake --build build --target build_external_libraries
cmake build
cmake --build build


if (echo "$CMAKE_FLAGS" | grep CMAKE_SYSTEM_NAME=iOS); then
    echo "iOS. Skipping install. Please open build/goopax_examples.xcodeproj and build manually."
else
    cmake --install build
fi
