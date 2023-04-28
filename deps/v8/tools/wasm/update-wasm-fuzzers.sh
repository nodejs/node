#!/bin/bash
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -ex

cd "$( dirname "${BASH_SOURCE[0]}" )"
cd ../..

BUILD_DIR=out/mk_wasm_fuzzer_corpus
CORPUS_DIR=test/fuzzer/wasm_corpus

rm -rf $BUILD_DIR $CORPUS_DIR
mkdir -p $CORPUS_DIR

# Build optdebug such that the --dump-wasm-module flag is available.
gn gen $BUILD_DIR --args='is_debug=true v8_optimized_debug=true target_cpu="x64" use_goma=true'
autoninja -C $BUILD_DIR

./tools/run-tests.py --outdir=$BUILD_DIR --extra-flags="--dump-wasm-module \
  --dump-wasm-module-path=./$CORPUS_DIR/"

rm -rf $BUILD_DIR

# Delete items over 20k.
find $CORPUS_DIR -type f -size +20k | xargs rm

# Upload changes.
cd $CORPUS_DIR/..
upload_to_google_storage.py -a -b v8-wasm-fuzzer wasm_corpus
