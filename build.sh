#!/bin/bash
g++ -o diff.so diff.cpp $(yed --print-cppflags) $(yed --print-ldflags) -w -fpermissive
