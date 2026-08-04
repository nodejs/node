#!/bin/bash
#
# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the chromium source repository LICENSE file.
#
# Given a zlib_bench executable and some data files, run zlib_bench --check
# over those data files, for all zlib types (gzip|zlib|raw) and compression
# levels 1..9 for each type. Example:
#
#  check.sh ./out/Release/zlib_bench [--check-binary] ~/snappy/testdata/*
#
# The --check-binary option modifies --check output: the compressed data is
# also written to the program output.

ZLIB_BENCH="$1" && shift

CHECK_TYPE="--check"
if [[ "${1}" == "--check-binary" ]]; then
  CHECK_TYPE="$1" && shift  # output compressed data too
fi

DATA_FILES="$*"

echo ${ZLIB_BENCH} | grep -E "/(zlib_bench|a.out)$" > /dev/null
if [[ $? != 0 ]] || [[ -z "${DATA_FILES}" ]]; then
  echo "usage: check.sh zlib_bench [--check-binary] files ..." >&2
  exit 1;
fi

for type in gzip zlib raw; do
  for level in $(seq 1 9); do
    ${ZLIB_BENCH} $type --compression $level ${CHECK_TYPE} ${DATA_FILES}
  done
done
