#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Build the ClusterFuzz blackbox-fuzzer archive.
#
# Usage: package.sh [output.zip]     (default: <v8>/out/regexp_fuzzer.zip)
#
# Archive root:
#   run.py               the fuzzer executable (--input_dir/--output_dir/
#                        --no_of_files)
#   harness.py harness.js grammar/  run.py dependencies
#   foozzie_launcher.py  js_fuzzer's launcher for existing correctness jobs
#
# Upload through ClusterFuzz's Fuzzers > Create/Edit page. Rebuild after
# changing grammar/ or the harness.

set -eu

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
V8_ROOT="$(cd "$DIR/../../.." && pwd)"
OUT="${1:-$V8_ROOT/out/regexp_fuzzer.zip}"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT

cp "$DIR/run.py" "$DIR/harness.py" "$DIR/harness.js" "$STAGE/"
mkdir "$STAGE/grammar"
cp "$DIR"/grammar/*.py "$STAGE/grammar/"
cp "$V8_ROOT/tools/clusterfuzz/js_fuzzer/foozzie_launcher.py" "$STAGE/"
chmod +x "$STAGE/run.py" "$STAGE/foozzie_launcher.py"

# Test the staged bundle outside the repository. On ClusterFuzz, APP_DIR is
# the build directory holding the foozzie experiment tables.
CHECK="$(mktemp -d)"
trap 'rm -rf "$STAGE" "$CHECK"' EXIT
(cd / && APP_DIR="$V8_ROOT/tools/clusterfuzz/foozzie" \
    PYTHONDONTWRITEBYTECODE=1 python3 "$STAGE/run.py" \
    --input_dir "$CHECK" --output_dir "$CHECK" --no_of_files 2 >/dev/null)
test -s "$CHECK/fuzz-0.js" && test -s "$CHECK/fuzz-1.js"
test -s "$CHECK/flags-0.js" && test -s "$CHECK/flags-1.js"

mkdir -p "$(dirname "$OUT")"
rm -f "$OUT"
# Exclude extra file attributes from the archive.
(cd "$STAGE" && zip -q -r -X "$OUT" .)
echo "wrote $OUT"
unzip -l "$OUT"
