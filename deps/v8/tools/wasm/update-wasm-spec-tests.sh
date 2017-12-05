#!/bin/bash
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

TOOLS_WASM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
V8_DIR="${TOOLS_WASM_DIR}/../.."

cd ${V8_DIR}

mkdir -p ./test/wasm-spec-tests/tests/
rm -rf ./test/wasm-spec-tests/tests/*

./tools/dev/gm.py x64.release d8

cd ${V8_DIR}/test/wasm-js/interpreter
make clean all

cd ${V8_DIR}/test/wasm-js/test/core

./run.py --wasm ${V8_DIR}/test/wasm-js/interpreter/wasm --js ${V8_DIR}/out/x64.release/d8

cp ${V8_DIR}/test/wasm-js/test/core/output/*.js ${V8_DIR}/test/wasm-spec-tests/tests

cd ${V8_DIR}/test/wasm-spec-tests
upload_to_google_storage.py -a -b v8-wasm-spec-tests tests


