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

log_and_run() {
  echo ">>" $*
  if ! $*; then
    echo "sub-command failed: $*"
    exit
  fi
}

###############################################################################
# Setup directories.
###############################################################################

TOOLS_WASM_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
V8_DIR="${TOOLS_WASM_DIR}/../.."
SPEC_TEST_DIR=${V8_DIR}/test/wasm-spec-tests
TMP_DIR=${SPEC_TEST_DIR}/tmp

JS_API_TEST_DIR=${V8_DIR}/test/wasm-js

log_and_run cd ${V8_DIR}

log_and_run rm -rf ${SPEC_TEST_DIR}/tests
log_and_run mkdir ${SPEC_TEST_DIR}/tests

log_and_run mkdir ${SPEC_TEST_DIR}/tests/proposals

log_and_run rm -rf ${TMP_DIR}
log_and_run mkdir ${TMP_DIR}

log_and_run rm -rf ${JS_API_TEST_DIR}/tests
log_and_run mkdir ${JS_API_TEST_DIR}/tests
log_and_run mkdir ${JS_API_TEST_DIR}/tests/proposals

###############################################################################
# Generate the spec tests.
###############################################################################

echo Process spec
log_and_run cd ${TMP_DIR}
log_and_run git clone https://github.com/WebAssembly/spec
log_and_run cd spec/interpreter

# The next step requires that ocaml is installed. See the README.md in
# https://github.com/WebAssembly/spec/tree/master/interpreter/.
log_and_run make clean opt

log_and_run cd ${TMP_DIR}/spec/test/core
log_and_run cp *.wast ${SPEC_TEST_DIR}/tests/

log_and_run ./run.py --wasm ${TMP_DIR}/spec/interpreter/wasm --out ${TMP_DIR}
log_and_run cp ${TMP_DIR}/*.js ${SPEC_TEST_DIR}/tests/

log_and_run cp -r ${TMP_DIR}/spec/test/js-api/* ${JS_API_TEST_DIR}/tests

###############################################################################
# Generate the proposal tests.
###############################################################################

repos='bulk-memory-operations reference-types js-types JS-BigInt-integration multi-value'

for repo in ${repos}; do
  echo "Process ${repo}"
  echo ">> Process core tests"
  log_and_run cd ${TMP_DIR}
  log_and_run git clone https://github.com/WebAssembly/${repo}
  # Compile the spec interpreter to generate the .js test cases later.
  log_and_run cd ${repo}/interpreter
  log_and_run make clean opt
  log_and_run cd ../test/core
  log_and_run mkdir ${SPEC_TEST_DIR}/tests/proposals/${repo}

  # Iterate over all proposal tests. Those which differ from the spec tests are
  # copied to the output directory and converted to .js tests.
  for abs_filename in ${TMP_DIR}/${repo}/test/core/*.wast; do
    rel_filename="$(basename -- $abs_filename)"
    test_name=${rel_filename%.wast}
    spec_filename=${TMP_DIR}/spec/test/core/${rel_filename}
    if [ ! -f "$spec_filename" ] || ! cmp -s $abs_filename $spec_filename ; then
      log_and_run cp ${rel_filename} ${SPEC_TEST_DIR}/tests/proposals/${repo}/
      log_and_run ./run.py --wasm ../../interpreter/wasm ${rel_filename} --out _build 2> /dev/null
    fi
  done
  log_and_run cp _build/*.js ${SPEC_TEST_DIR}/tests/proposals/${repo}/

  echo ">> Process js-api tests"
  log_and_run mkdir ${JS_API_TEST_DIR}/tests/proposals/${repo}
  log_and_run cp -r ${TMP_DIR}/${repo}/test/js-api/* ${JS_API_TEST_DIR}/tests/proposals/${repo}
  # Delete duplicate tests
  log_and_run cd ${JS_API_TEST_DIR}/tests
  for spec_test_name in $(find ./ -name '*.any.js' -not -wholename '*/proposals/*'); do
    proposal_test_name="proposals/${repo}/${spec_test_name}"
    if [ -f "$proposal_test_name" ] && cmp -s $spec_test_name $proposal_test_name ; then
      log_and_run rm $proposal_test_name
    elif [ -f "$proposal_test_name" ]; then
      echo "keep" $proposal_test_name
    fi
  done
done

###############################################################################
# Report and cleanup.
###############################################################################

cd ${SPEC_TEST_DIR}
echo
echo "The following files will get uploaded:"
ls -R tests
echo

cd ${JS_API_TEST_DIR}
ls -R tests
echo

log_and_run rm -rf ${TMP_DIR}

###############################################################################
# Upload all spec tests.
###############################################################################

echo "****************************************************************************"
echo "* For the following command you first have to authenticate with google cloud"
echo "* storage. For that you have to execute"
echo "*"
echo "* > gsutil.py config"
echo "*"
echo "* When the script asks you for your project-id, use 0."
echo "****************************************************************************"
log_and_run cd ${SPEC_TEST_DIR}
log_and_run upload_to_google_storage.py -a -b v8-wasm-spec-tests tests

log_and_run cd ${JS_API_TEST_DIR}
log_and_run upload_to_google_storage.py -a -b v8-wasm-spec-tests tests
