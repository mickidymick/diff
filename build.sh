#!/bin/bash
# g++ -o diff.so diff.cpp -std=c++11 $(yed --print-cppflags) $(yed --print-ldflags) -w -fpermissive
make
