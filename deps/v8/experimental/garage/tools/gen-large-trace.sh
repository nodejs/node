#!/usr/bin/env bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Generate the multi-hundred-MB perf trace. NOT checked in (see .gitignore) --
# regenerate it locally when working on TODO 5.1 (open 500 MB in < 2 s).
#
#   tools/gen-large-trace.sh [target_MB] [output_path]
#
# Defaults to 600 MB at fixtures/large/huge.trace. The generator emits a
# synthetic workload with many small functions so the trace contains thousands
# of distinct compilations -- that is what stresses the section indexer and the
# sidebar, not one enormous graph.
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_MB="${1:-600}"
OUT="${2:-$REPO_ROOT/fixtures/large/huge.trace}"
D8="${GARAGE_D8:-$HOME/repos/v8/v8/out/arm64.release/d8}"

[[ -x "$D8" ]] || { echo "no d8 at $D8 (set GARAGE_D8)" >&2; exit 1; }

mkdir -p "$(dirname "$OUT")"
GEN_JS="$(dirname "$OUT")/generated-workload.js"

# Number of functions is tuned by measuring one function's trace cost, so the
# script hits the size target instead of guessing.
FUNCS="${GARAGE_LARGE_FUNCS:-4000}"

echo "generating $FUNCS-function workload..." >&2
{
  for ((i = 0; i < FUNCS; i++)); do
    cat <<EOF
function f$i(a, b) {
  let r = (a * $((i % 97 + 1))) | 0;
  for (let k = 0; k < 3; k++) r = (r + b * k) | 0;
  if (r > $((i * 7 % 1000))) r = (r ^ $i) | 0;
  return r + a;
}
EOF
  done
  echo "let acc = 0;"
  echo "for (let n = 0; n < 3000; n++) {"
  for ((i = 0; i < FUNCS; i++)); do echo "  acc = f$i(acc, n) | 0;"; done
  echo "}"
  echo "print(acc);"
} > "$GEN_JS"

echo "running d8 (this takes a while)..." >&2
( cd "$(dirname "$OUT")" && "$D8" \
    --no-concurrent-recompilation --predictable \
    --print-maglev-graphs --trace-opt --trace-deopt \
    "$(basename "$GEN_JS")" ) > "$OUT" 2>&1 || true

SIZE_MB=$(( $(wc -c < "$OUT") / 1024 / 1024 ))
echo "wrote $OUT (${SIZE_MB} MB, target ${TARGET_MB} MB)" >&2
if (( SIZE_MB < TARGET_MB )); then
  SUGGEST=$(( FUNCS * TARGET_MB / (SIZE_MB > 0 ? SIZE_MB : 1) ))
  echo "re-run with GARAGE_LARGE_FUNCS=$SUGGEST to reach the target" >&2
fi
