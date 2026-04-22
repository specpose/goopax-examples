#!/bin/sh
gcc -D'DEBUG=1' -std=c99 -Wno-cpp `/usr/bin/python3-config --cflags` `/usr/bin/python3-config --ldflags --embed` -x c $1
