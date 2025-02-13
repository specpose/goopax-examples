#!/bin/bash

set -e

export CMAKE_BUILD_PARALLEL_LEVEL="$(getconf _NPROCESSORS_ONLN)"

#may require cmake>=3.21
cmake "$@" -B build/$build_type/$ABI src
cmake --build build/$build_type/$ABI --target build_glatter
cmake --build build/$build_type/$ABI --target build_eigen
cmake --build build/$build_type/$ABI --target build_sdl
cmake --build build/$build_type/$ABI --target build_boost
cmake --build build/$build_type/$ABI --target build_opencv
cmake --build build/$build_type/$ABI
cmake --install build/$build_type/$ABI --prefix data_local_tmp
