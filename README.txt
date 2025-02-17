Building Example Programs
=========================

Running the script

    ./build-all.sh

will fetch and build all required libraries (SDL3, OpenCV, boost, Eigen, glatter), unless they are already installed on your system, and then continue to build the example programs.

If you prefer not to build external libraries, you can start cmake by hand:

    cmake -B build src
    cmake --build build

In this case, only those examples are build, for which all required libraries are found.


Windows
-------
To build the example programs on windows, install the following applications:
- visual studio 2022
- cmake
- git (with git bash)
Use git bash to run the build script

    ./build-all.sh


MacOS
-----
To build the example programs for MacOS, install the following applications:
- Xcode
- cmake
Open a terminal.
Set the PATH variable so that the cmake executable is found.
Then run

    ./build-all.sh


iOS
---
In addition to the steps required for MacOS, you also need to specify your developer team id (see https://developer.apple.com/help/account/manage-your-team/locate-your-team-id), and to pass some additional arguments. The following line will build for minimum iOS version 15.

    ./build-all.sh -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=15 -DAPPLE_DEVELOPER_TEAM=<your_developer_team_id>

Then, open build/goopax_examples.xcodeproj and install the programs manually.


Android
-------
The following has been tested with requirements installed from the Android Studio SDK Installer for Linux:
- android ndk
- android sdk
- cmake, including ninja from the ndk.
Set the PATH variable so that both ninja and corresponding cmake are found.

    export PATH=$HOME/Android/Sdk/cmake/3.22.1/bin:$PATH

This represents the minimal system requirements to build the text-based examples.

    build_type="debug" ABI="arm64-v8a" platform_version_string="android-28" android_sdk="$HOME/Android/Sdk" ndk_version="21.4.7075529" bash -c './build-all.sh -DCMAKE_SYSTEM_NAME="Android" -G "Ninja" -DANDROID_ABI="$ABI" -DANDROID_PLATFORM="$platform_version_string" -DANDROID_NDK="$android_sdk/ndk/$ndk_version" -DCMAKE_TOOLCHAIN_FILE="$android_sdk/ndk/$ndk_version/build/cmake/android.toolchain.cmake" -DCMAKE_FIND_ROOT_PATH="$PWD/../;$PWD/build/$build_type/$ABI/ext/boost;$PWD/build/$build_type/$ABI/ext/sdl3;$PWD/build/$build_type/$ABI/ext/eigen" -DGOOPAX_DRAW_WITH_OPENGL=0 -DGOOPAX_DRAW_WITH_METAL=0'
