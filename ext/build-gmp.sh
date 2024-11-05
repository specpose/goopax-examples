#!/bin/bash

set -e
set -o nounset
set -o pipefail

PREFIX="$PWD/dist/gmp"

mkdir -p src
curl https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz -o src/gmp-6.3.0.tar.xz
shasum -a 256 src/gmp-6.3.0.tar.xz | grep  a3c2b80201b89e68616f4ad30bc66aee4927c3ce50e33929ca819d5c43538898

mkdir -p build
cd build
tar -xf ../src/gmp-6.3.0.tar.xz
cd gmp-6.3.0
./configure --prefix="$PREFIX"
make -j10 install
#git clone ~/opt/src/git/eigen src/eigen -b 3.4.0
#cmake -B build/gmp src/gmp -DCMAKE_INSTALL_PREFIX="$PWD/dist/gmp"
#cmake --build build/gmp --target install
