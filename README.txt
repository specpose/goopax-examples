Building Example Programs
=========================

Run script `build-all.sh` to build required libraries and the example programs.


Windows
-------
To build the example programs on windows, install the following applications:
- visual studio 2022
- cmake
- git (with git bash)
Use git bash to run the build script `./build-all.sh`.


MacOS
-----
To build the example programs for MacOS or iOS, install the following applications:
- Xcode
- cmake
Open a terminal.
Set the PATH variable so that the cmake executable is found.
Then run `./build-all.sh`


iOS
---
In addition to the steps required for MacOS, you also need to set the APPLE_DEVELOPER_TEAM environment variable to your developer team id (see https://developer.apple.com/help/account/manage-your-team/locate-your-team-id), and to pass some additional arguments. The following line will build for minimum iOS version 15.

APPLE_DEVELOPER_TEAM=<your developer team id> ./build-all.sh -DCMAKE_SYSTEM_NAME=iOS -G Xcode -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=15

Then, open build/goopax_examples.xcodeproj and install the programs manually.

