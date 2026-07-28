#!/bin/sh
gcc -D'MODULE_LIBRARY=1' -shared -fPIC -std=c99 -Wno-cpp `/usr/bin/python3-config --cflags` `/usr/bin/python3-config --ldflags --embed` -x c -o build/$1`/usr/bin/python-config --extension-suffix
` src/$1.cpp
