#!/bin/bash

set -e

if  [ "$(uname -o)" == "Msys" ]; then
  CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_CONFIGURATION_TYPES=Debug -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebug"
fi

export CMAKE_BUILD_PARALLEL_LEVEL="$(getconf _NPROCESSORS_ONLN)"

cmake "$@" $CMAKE_FLAGS -B build src
cmake --build build --target build_external_libraries
cmake build
cmake --build build
