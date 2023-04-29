#!/bin/bash
set -e
name=$1
name=${name%.}
name=${name%.cc}
name=${name%.exe}
echo Compiling $name.cc to $name.exe
i686-w64-mingw32-g++.exe -static -std=c++11 $name.cc -o $name.exe
i686-w64-mingw32-strip $name.exe
