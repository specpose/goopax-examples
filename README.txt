Building Example Programs
=========================

Run script `build-all.sh` to build required libraries and the example programs.


Windows
-------
To build the example programs on windows, install the following applications:
- visual studio 2022
- cmake
- git (with git bash)
Use git bash to run the build scripts `build-external-libraries.sh` and `build.sh`.


iOS
---
You need a developer account at https://developer.apple.com

- Open src/cmake/common.cmake, set the DEVELOPMENT_TEAM property to your developer ID (you can see it at https://developer.apple.com -> account -> certificates on the top right).
- Run build-external-libraries-for-ios.sh
- Run build-for-ios.sh. 
- Open build/goopax_examples.xcodeproj and build manually.

