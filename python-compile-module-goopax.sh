#!/bin/sh
g++ -D'DEBUG=1' -D'MODULE_LIBRARY=1' -shared -fPIC -std=c++17 `/usr/bin/python3-config --cflags` -I../goopax-6.0.0-Linux-x86_64/include/goopax-6.0 -L../goopax-6.0.0-Linux-x86_64/lib64 `/usr/bin/python3-config --ldflags --embed` -lgoopax -o build/$1`/usr/bin/python-config --extension-suffix` src/$1.cpp
