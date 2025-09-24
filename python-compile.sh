#!/bin/sh
/usr/bin/python3-config --cflags
/usr/bin/python3-config --ldflags --embed
#gcc -std=c11 -Wno-cpp -I/usr/include/python3.13/ -L/usr/lib64 -lpython3.13 -ldl -lm -x c $1
gcc -D'DEBUG=1' -std=c11 -Wno-cpp `/usr/bin/python3-config --cflags` `/usr/bin/python3-config --ldflags --embed` -x c $1
