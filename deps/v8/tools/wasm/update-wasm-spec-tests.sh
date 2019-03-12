#!/bin/bash
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Exit immediately if a command exits with a non-zero status.
set -e

# Treat unset variables as an error when substituting.
set -u

# return value of a pipeline is the status of the last command to exit with a
# non-zero status, or zero if no command exited with a non-zero status
set -o pipefail

TOOLS_WASM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
V8_DIR="${TOOLS_WASM_DIR}/../.."
SPEC_TEST_DIR=${V8_DIR}/test/wasm-spec-tests

cd ${V8_DIR}

rm -rf ${SPEC_TEST_DIR}/tests
mkdir ${SPEC_TEST_DIR}/tests

rm -rf ${SPEC_TEST_DIR}/tmp
mkdir ${SPEC_TEST_DIR}/tmp

./tools/dev/gm.py x64.release d8

cd ${V8_DIR}/test/wasm-js/data/interpreter

# The next step requires that ocaml is installed. See the README.md in
# ${V8_DIR}/test/wasm-js/data/interpreter/.
make clean all

cd ${V8_DIR}/test/wasm-js/data/test/core


./run.py --wasm ${V8_DIR}/test/wasm-js/data/interpreter/wasm --js ${V8_DIR}/out/x64.release/d8 --out ${SPEC_TEST_DIR}/tmp
cp ${SPEC_TEST_DIR}/tmp/*.js ${SPEC_TEST_DIR}/tests/
rm -rf ${SPEC_TEST_DIR}/tmp

cd ${SPEC_TEST_DIR}
echo
echo "The following files will get uploaded:"
ls tests
echo

# For the following command you first have to authenticate with google cloud
# storage. For that you have to execute
#
# > gsutil.py config
#
# When the script asks you for your project-id, use 0.
upload_to_google_storage.py -a -b v8-wasm-spec-tests tests
