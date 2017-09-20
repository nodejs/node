#!/bin/bash
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

TOOLS_WASM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${TOOLS_WASM_DIR}/../..

rm -rf test/fuzzer/wasm_corpus

tools/dev/gm.py x64.release all

mkdir -p test/fuzzer/wasm_corpus

# wasm
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=release --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm_corpus/" unittests
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=release --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm_corpus/" wasm-spec-tests/*
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=release --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm_corpus/" mjsunit/wasm/*
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=release --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm_corpus/" \
  $(cd test/; ls cctest/wasm/test-*.cc | \
  sed -es/wasm\\///g | sed -es/[.]cc/\\/\\*/g)

# Delete items over 20k.
for x in $(find ./test/fuzzer/wasm_corpus/ -type f -size +20k)
do
  rm $x
done

# Upload changes.
cd test/fuzzer
upload_to_google_storage.py -a -b v8-wasm-fuzzer wasm_corpus
