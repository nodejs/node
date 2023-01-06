#!/bin/bash -eu
# Copyright 2019 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

# This script is meant to be run by
# https://github.com/google/oss-fuzz/blob/master/projects/libzip/Dockerfile

mkdir build
cd build
cmake -DBUILD_SHARED_LIBS=OFF -DENABLE_GNUTLS=OFF -DENABLE_MBEDTLS=OFF -DENABLE_OPENSSL=OFF -DBUILD_TOOLS=OFF -DENABLE_LZMA=OFF ..
make -j$(nproc)

$CXX $CXXFLAGS -std=c++11 -I. -I../lib \
    $SRC/libzip/regress/zip_read_fuzzer.cc \
    -o $OUT/zip_read_fuzzer \
    $LIB_FUZZING_ENGINE $SRC/libzip/build/lib/libzip.a -lz

find $SRC/libzip/regress -name "*.zip" | \
     xargs zip $OUT/zip_read_fuzzer_seed_corpus.zip

cp $SRC/libzip/regress/zip_read_fuzzer.dict $OUT/

