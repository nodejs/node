#!/bin/bash -eu

autoreconf -i
./configure --disable-dependency-tracking
make -j$(nproc) check TESTS=""

$CXX $CXXFLAGS -std=c++17 -I. \
     fuzz/parser.cc -o $OUT/parser \
     $LIB_FUZZING_ENGINE .libs/liburlparse.a .libs/libhttp-parser.a

zip -j $OUT/parser_seed_corpus.zip fuzz/corpus/parser/*
