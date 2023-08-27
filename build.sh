#!/bin/bash
g++ -o diff.so diff.cpp $(yed --print-cflags) $(yed --print-ldflags) -w -fpermissive
# g++ -o diff.so diff.cpp $(yed --print-cflags) $(yed --print-ldflags) -Wno-error -fpermissive
