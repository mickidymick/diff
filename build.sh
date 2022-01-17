#!/bin/bash
gcc -o diff.so diff.c $(yed --print-cflags) $(yed --print-ldflags)
