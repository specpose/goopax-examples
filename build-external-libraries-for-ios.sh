#!/bin/bash

set -e

MIN_IOS_VERSION=13

XCODE_ROOT="$(xcode-select -print-path)"
#export CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_TOOLCHAIN_FILE=$PWD/toolchains/toolchain-ios.cmake -G Xcode"
export CMAKE_FLAGS="$CMAKE_FLAGS -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=$MIN_IOS_VERSION -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=$MIN_IOS_VERSION -G Xcode -DBOOST_EXCLUDE_LIBRARIES=process;context;coroutine;fiber;asio;log -DWITH_JPEG=0 -DWITH_PNG=0"

export MAKEFILE_CFLAGS="-mios-version-min=$MIN_IOS_VERSION -arch arm64 -isysroot $XCODE_ROOT/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"
export MAKEFILE_CC="$XCODE_ROOT/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang"
export MAKEFILE_CONFIGURE_FLAGS="--host=$(uname -m)-apple-darwin --disable-assembly"

./build-external-libraries.sh
