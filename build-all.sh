#!/bin/bash

set -e

export CMAKE_BUILD_PARALLEL_LEVEL="$(getconf _NPROCESSORS_ONLN)"

cmake "$@" $CMAKE_FLAGS -B build src
cmake --build build --target build_external_libraries
cmake build
cmake --build build
