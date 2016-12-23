#!/bin/bash
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

TOOLS_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

cd ${TOOLS_DIR}/..

rm -rf test/fuzzer/wasm
rm -rf test/fuzzer/wasm_asmjs

make x64.debug -j

mkdir -p test/fuzzer/wasm
mkdir -p test/fuzzer/wasm_asmjs

# asm.js
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=debug --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm_asmjs/" mjsunit/wasm/asm*
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=debug --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm_asmjs/" mjsunit/asm/*
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=debug --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm_asmjs/" mjsunit/regress/asm/*
# WASM
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=debug --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm/" unittests
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=debug --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm/" mjsunit/wasm/*
./tools/run-tests.py -j8 --variants=default --timeout=10 --arch=x64 \
  --mode=debug --no-presubmit --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./test/fuzzer/wasm/" \
  $(cd test/; ls cctest/wasm/test-*.cc | \
  sed -es/wasm\\///g | sed -es/[.]cc/\\/\\*/g)

# Delete items over 20k.
for x in $(find ./test/fuzzer/wasm/ -type f -size +20k)
do
  rm $x
done
for x in $(find ./test/fuzzer/wasm_asmjs/ -type f -size +20k)
do
  rm $x
done

# Upload changes.
cd test/fuzzer
upload_to_google_storage.py -a -b v8-wasm-fuzzer wasm
upload_to_google_storage.py -a -b v8-wasm-asmjs-fuzzer wasm_asmjs
