#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Makes the bug bundle of one fix. The script reverts the fix on a base
# revision and builds the bits d8, the counters d8 and the d8 of Fuzzilli. It
# copies them with their data files, and writes the change-based fuzzing
# targets of the revert.
#
# Usage: make_bundle.sh FIX PRESET
#   FIX     the commit of the fix
#   PRESET  the Redline preset of the bug, for example maglev or turbolev
# Environment:
#   BB_ROOT        the directory of the bundles (required). Use a disk, not a
#                  file system in memory.
#   BB_V8          a second V8 checkout for the builds (required). It must
#                  have no local changes.
#   BASE           the base revision (default origin/main)
#   BITS_OUT       the build directory of the bits d8 in BB_V8
#                  (default out/fuzzbuild)
#   COUNTERS_OUT   the build directory of the counters d8 in BB_V8
#                  (default out/rl_counters, as in the change-based fuzzing
#                  section of TUTORIAL.md)
#   FUZZILLI_OUT   the build directory of the d8 of Fuzzilli in BB_V8, with
#                  v8_fuzzilli=true (default out/fuzzilli)
#   REDLINE_TOOLS  the experimental/redline/tools directory of the Redline
#                  checkout (default: found from this script)
#
# The script checks out BASE in BB_V8 without a branch, commits the revert and
# tags it bb/FIX. After the build, it checks out the old revision of BB_V8
# again. A new build of a bundle replaces bits/, counters/, fuzzilli/ and
# targets.json.
set -euo pipefail

if [ $# -ne 2 ]; then
  awk 'NR >= 6 && /^#/ { sub(/^# ?/, ""); print; next } NR >= 6 { exit }' "$0"
  exit 2
fi
FIX=$1 PRESET=$2
: "${BB_ROOT:?set BB_ROOT to the directory of the bundles}"
: "${BB_V8:?set BB_V8 to a second V8 checkout for the builds}"
HERE=$(cd "$(dirname "$0")" && pwd)
TOOLS=$(cd "${REDLINE_TOOLS:-$HERE/../../../tools}" && pwd)
BASE=${BASE:-origin/main}
BITS_OUT=${BITS_OUT:-out/fuzzbuild}
COUNTERS_OUT=${COUNTERS_OUT:-out/rl_counters}
FUZZILLI_OUT=${FUZZILLI_OUT:-out/fuzzilli}

BB_V8=$(cd "$BB_V8" && pwd -P)
if [ "$(git -C "$TOOLS" rev-parse --show-toplevel)" = "$BB_V8" ]; then
  echo "BB_V8 must be another checkout than the one of $TOOLS," >&2
  echo "because the script checks out other revisions in BB_V8." >&2
  exit 1
fi
cd "$BB_V8"
# The LLVM tools of the toolchain that builds d8. Without llvm-objdump,
# change_targets.py keeps the edges that only lead to a fatal error.
LLVM=$BB_V8/third_party/llvm-build/Release+Asserts/bin
for tool in llvm-objdump llvm-symbolizer; do
  [ -x "$LLVM/$tool" ] || { echo "no $LLVM/$tool" >&2; exit 1; }
done
grep -q 'inline-bool-flag' "$BITS_OUT/args.gn" 2>/dev/null || {
  echo "$BB_V8/$BITS_OUT/args.gn must set sanitizer_coverage_flags = \"inline-bool-flag\"" >&2
  exit 1
}
grep -q 'inline-8bit-counters,pc-table' "$COUNTERS_OUT/args.gn" 2>/dev/null || {
  echo "$BB_V8/$COUNTERS_OUT/args.gn must set sanitizer_coverage_flags = \"inline-8bit-counters,pc-table\"" >&2
  exit 1
}
for out in "$BITS_OUT" "$COUNTERS_OUT"; do
  grep -q 'v8_redline *= *true' "$out/args.gn" 2>/dev/null || {
    echo "$BB_V8/$out/args.gn must set v8_redline = true" >&2
    exit 1
  }
done
grep -q 'v8_fuzzilli *= *true' "$FUZZILLI_OUT/args.gn" 2>/dev/null || {
  echo "$BB_V8/$FUZZILLI_OUT/args.gn must set v8_fuzzilli = true" >&2
  exit 1
}
if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "$BB_V8 has local changes" >&2
  exit 1
fi

FIX=$(git rev-parse --short=11 "$FIX^{commit}")
ORIG=$(git symbolic-ref -q --short HEAD || git rev-parse HEAD)
restore() {
  git revert --abort > /dev/null 2>&1 || true
  git checkout -q "$ORIG"
}
trap restore EXIT

git checkout -q --detach "$BASE"
# The counters build needs the coverage counters of d8 (change-based fuzzing).
if ! grep -q 'REDLINE_PC_TABLE' experimental/redline/d8/cov.cc 2>/dev/null; then
  echo "$BASE has no coverage counters in experimental/redline/d8/cov.cc." >&2
  echo "Use a base revision that has the d8 side of change-based fuzzing." >&2
  exit 1
fi
if ! git revert --no-edit "$FIX" > /dev/null 2>&1; then
  echo "$FIX: the revert does not apply on $BASE. Choose another fix." >&2
  exit 3
fi
git tag -f "bb/$FIX" > /dev/null
echo "$FIX: revert $(git rev-parse --short=11 HEAD) on $(git rev-parse --short=11 HEAD~1)"

autoninja -C "$BITS_OUT" d8
autoninja -C "$COUNTERS_OUT" d8
autoninja -C "$FUZZILLI_OUT" d8

DST=$BB_ROOT/$FIX
rm -rf "$DST/bits" "$DST/counters" "$DST/fuzzilli" "$DST/targets.json"
mkdir -p "$DST/bits" "$DST/counters" "$DST/fuzzilli"
for f in d8 snapshot_blob.bin icudtl.dat; do
  if [ -f "$BITS_OUT/$f" ]; then cp "$BITS_OUT/$f" "$DST/bits/"; fi
  if [ -f "$COUNTERS_OUT/$f" ]; then cp "$COUNTERS_OUT/$f" "$DST/counters/"; fi
  if [ -f "$FUZZILLI_OUT/$f" ]; then cp "$FUZZILLI_OUT/$f" "$DST/fuzzilli/"; fi
done
echo "$PRESET" > "$DST/preset"
echo "$FIX" > "$DST/fix"
git rev-parse HEAD > "$DST/revision"
python3 "$TOOLS/edge_map.py" --d8="$DST/counters/d8" \
    --symbolizer="$LLVM/llvm-symbolizer"
python3 "$TOOLS/change_targets.py" --repo="$BB_V8" --d8="$DST/counters/d8" \
    --objdump="$LLVM/llvm-objdump" --base=HEAD~1 --head=HEAD \
    --out="$DST/targets.json"
echo "$FIX: bundle in $DST. Now do the checks of SKILL.md section 5."
