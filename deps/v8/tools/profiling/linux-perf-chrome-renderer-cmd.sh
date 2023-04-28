#!/usr/bin/env bash
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

PERF_DATA_DIR="."
PERF_DATA_PREFIX="chrome_renderer"
RENDERER_ID="0"
for i in "$@"; do
  case $i in
    -h|--help)
      echo "Usage: path/to/chrome --renderer-cmd-prefix='$0 [OPTION]' [CHROME OPTIONS]"
      echo "This script is mostly used in conjuction with linux_perf.py to run linux-perf"
      echo "for each renderer process."
      echo "It generates perf.data files that can be read by pprof or linux-perf."
      echo ""
      echo 'File naming: ${OUT_DIR}/${PREFIX}_${PARENT_PID}_${RENDERER_ID}.perf.data'
      echo ""
      echo "Options:"
      echo "  --perf-data-dir=OUT_DIR    Change the location where perf.data is written."
      echo "                             Default: '$PERF_DATA_DIR'"
      echo "  --perf-data-prefix=PREFIX  Set a custom prefex for all generated perf.data files."
      echo "                             Default: '$PERF_DATA_PREFIX'"
      exit
      ;;
    --perf-data-dir=*)
      PERF_DATA_DIR="${i#*=}"
      shift
    ;;
    --perf-data-prefix=*)
      PERF_DATA_PREFIX="${i#*=}"
      shift
    ;;
    --renderer-client-id=*)
      # Don't shift this option since it is passed in (and used by) chrome.
      RENDERER_ID="${i#*=}"
    ;;
    *)
      ;;
  esac
done

PERF_OUTPUT="$PERF_DATA_DIR/${PERF_DATA_PREFIX}_${PPID}_${RENDERER_ID}.perf.data"
perf record --call-graph=fp --clockid=mono --freq=max --output="${PERF_OUTPUT}" -- $@
