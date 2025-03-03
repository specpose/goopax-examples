#!/bin/bash
build_type="debug" ABI="arm64-v8a" platform_version_string="android-28" android_sdk="$HOME/Android/Sdk" ndk_version="21.4.7075529" bash -c 'cmake -Wno-dev -B build src -DCMAKE_SYSTEM_NAME="Android" -G "Ninja" -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="$platform_version_string" -DANDROID_NDK="$android_sdk/ndk/$ndk_version" -DCMAKE_TOOLCHAIN_FILE="$android_sdk/ndk/$ndk_version/build/cmake/android.toolchain.cmake" -DCMAKE_FIND_ROOT_PATH="$PWD/../;$PWD/build/$build_type/$ABI/ext/boost;$PWD/build/$build_type/$ABI/ext/opencv;$PWD/build/$build_type/$ABI/ext/sdl3;$PWD/build/$build_type/$ABI/ext/eigen" -DGOOPAX_DRAW_WITH_OPENGL=0 -DGOOPAX_DRAW_WITH_METAL=0'
