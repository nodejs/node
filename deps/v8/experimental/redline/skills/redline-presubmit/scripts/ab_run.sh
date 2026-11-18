#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Compares fuzzing before and after a change (CONTRIBUTE.md section 10).
# The script builds Redline at the base revision and at HEAD. It copies each
# binary with its data into a work directory. Then it runs the two sessions at
# the same time, with the same d8, flags and seed, and compares their coverage.
#
# Run it from the root of the V8 checkout. Commit your change first: the
# script checks out the base revision for some time, and it refuses to start
# if tracked files have changes.
#
# Usage: ab_run.sh --base=REV [--duration=SEC] [--jobs=N] [--work=DIR]
#                  [--out=DIR] [--d8=PATH] [-- FUZZ_FLAG...]
#   --base=REV      the revision before the change (required)
#   --duration=SEC  the length of each session (default: 3600)
#   --jobs=N        the workers of each session (default: 8)
#   --work=DIR      the work directory (default: a new /tmp/redline-ab.*)
#   --out=DIR       the Redline build directory (default: out/redline)
#   --d8=PATH       the fuzzing build of d8 (default: out/fuzzbuild/d8)
#   FUZZ_FLAG       more flags for both sessions, for example --preset=maglev

set -u
BASE=""
DURATION=3600
JOBS=8
WORK="${AB_WORK:-}"
OUT=out/redline
D8=out/fuzzbuild/d8
EXTRA=()
while [ $# -gt 0 ]; do
  case "$1" in
    --base=*) BASE="${1#--base=}" ;;
    --duration=*) DURATION="${1#--duration=}" ;;
    --jobs=*) JOBS="${1#--jobs=}" ;;
    --work=*) WORK="${1#--work=}" ;;
    --out=*) OUT="${1#--out=}" ;;
    --d8=*) D8="${1#--d8=}" ;;
    --) shift; EXTRA=("$@"); break ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
  shift
done
if [ -z "$BASE" ]; then
  echo "--base=REV is required." >&2
  exit 2
fi
if [ ! -d experimental/redline ]; then
  echo "Run this script from the root of the V8 checkout." >&2
  exit 2
fi
if [ ! -x "$D8" ]; then
  echo "No d8 at $D8 (TUTORIAL.md section 1 tells how to build it)." >&2
  exit 2
fi
if ! git diff --quiet HEAD --; then
  echo "Tracked files have changes. Commit or stash them first." >&2
  exit 2
fi
BASE_SHA="$(git rev-parse --verify --quiet "$BASE^{commit}")"
if [ -z "$BASE_SHA" ]; then
  echo "Unknown revision: $BASE" >&2
  exit 2
fi
if [ -z "$WORK" ]; then
  WORK="$(mktemp -d /tmp/redline-ab.XXXXXX)"
fi
mkdir -p "$WORK/tools" "$WORK/base" "$WORK/after"

# The checkout of the base revision can remove or change this script, so the
# script continues from a copy in the work directory.
if [ -z "${AB_COPY:-}" ]; then
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  cp "$SCRIPT_DIR/ab_run.sh" "$SCRIPT_DIR/ab_compare.py" "$WORK/tools/"
  AB_COPY=1 AB_WORK="$WORK" exec bash "$WORK/tools/ab_run.sh" \
    --base="$BASE" --duration="$DURATION" --jobs="$JOBS" --out="$OUT" \
    --d8="$D8" -- "${EXTRA[@]}"
fi

ORIG="$(git symbolic-ref -q --short HEAD || git rev-parse HEAD)"
ON_BASE=0  # 1 while the checkout is at the base revision.
PIDS=()
cleanup() {
  for pid in "${PIDS[@]}"; do kill -INT "$pid" 2> /dev/null; done
  if [ "$ON_BASE" = 1 ]; then
    git checkout -q "$ORIG" && echo "Restored $ORIG."
  fi
}
trap cleanup EXIT
trap 'exit 130' INT TERM

# Builds Redline and copies the binary, its V8 startup files, the presets and
# the test data to DIR/bin. The binary reads presets/ and testdata/ next to
# itself before it looks in the source tree.
build_and_copy() {  # build_and_copy DIR
  echo "Building $(git log -1 --format='%h %s' HEAD) ..."
  if ! autoninja -C "$OUT" redline > "$1/build.log" 2>&1; then
    echo "The build failed: see $1/build.log" >&2
    exit 1
  fi
  mkdir -p "$1/bin"
  cp "$OUT/redline" "$OUT/snapshot_blob.bin" "$OUT/icudtl.dat" "$1/bin/" &&
    cp -r experimental/redline/presets experimental/redline/testdata \
      "$1/bin/" || exit 1
  git log -1 --format='%H %s' HEAD > "$1/revision.txt"
}

ON_BASE=1
git checkout -q --detach "$BASE_SHA" || exit 1
build_and_copy "$WORK/base"
git checkout -q "$ORIG" || exit 1
ON_BASE=0
build_and_copy "$WORK/after"

COMMON=(--d8="$D8" "--d8-extra-args=--single-threaded --random-seed=42"
        --preset=default --jobs="$JOBS" --seed=42 --duration="$DURATION"
        --stats-interval=60 --no-component-stats "${EXTRA[@]}")
echo "Flags of both sessions: ${COMMON[*]}" | tee "$WORK/flags.txt"
echo "Running two sessions of $DURATION s in $WORK ..."
for side in base after; do
  "$WORK/$side/bin/redline" fuzz "${COMMON[@]}" --out="$WORK/$side/run" \
    > "$WORK/$side/fuzz.log" 2>&1 &
  PIDS+=($!)
done
STATUS=0
for pid in "${PIDS[@]}"; do wait "$pid" || STATUS=1; done
PIDS=()
if [ "$STATUS" != 0 ]; then
  echo "A session failed: see $WORK/base/fuzz.log and $WORK/after/fuzz.log" >&2
fi

python3 "$WORK/tools/ab_compare.py" "$WORK/base/run/events.jsonl" \
  "$WORK/after/run/events.jsonl" | tee "$WORK/compare.txt"
echo "Base:  $(cat "$WORK/base/revision.txt")"
echo "After: $(cat "$WORK/after/revision.txt")"
echo "Work directory: $WORK"
exit "$STATUS"
