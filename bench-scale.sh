#!/usr/bin/env bash
# TEMPORARY benchmarking helper — not part of this change, remove before merge.
# Reuses the already-built main/branch binaries from bench-branch.sh (must run
# that first) and re-runs the SEA-build+time steps at several distinct-function
# counts, to see whether the % improvement holds at realistic sizes or is an
# artifact of the original 50,000-function test.
set -uo pipefail

BIN_DIR="$HOME/bench-binaries"
RESULTS_DIR="$HOME/bench-results-scale"
WARMUP_RUNS=3
TIMED_RUNS=15
SIZES=(100 500 2000 10000 50000)

log() { echo "[$(date '+%H:%M:%S')] $*"; }

if [ ! -x "$BIN_DIR/node-main" ] || [ ! -x "$BIN_DIR/node-branch" ]; then
  log "FATAL: expected $BIN_DIR/node-main and node-branch — run bench-branch.sh first."
  exit 1
fi

mkdir -p "$RESULTS_DIR"
cd "$RESULTS_DIR"

run_series() {
  local label="$1"
  local bin="./sea-bench-$label"
  for i in $(seq 1 "$WARMUP_RUNS"); do "$bin" >/dev/null 2>/dev/null || true; done
  local -a samples=()
  for i in $(seq 1 "$TIMED_RUNS"); do
    local start end ms
    start=$(date +%s%N)
    "$bin" >/dev/null 2>/dev/null
    end=$(date +%s%N)
    ms=$(( (end - start) / 1000000 ))
    samples+=("$ms")
  done
  printf '%s\n' "${samples[@]}" | sort -n | awk -v label="$label" '
    { sum += $1; a[NR] = $1 }
    END {
      n = NR; mean = sum / n
      printf "%s %.1f", label, mean
    }'
}

echo "n_functions main_mean_ms branch_mean_ms pct_faster" > summary.txt

for n in "${SIZES[@]}"; do
  log "=== n=$n ==="
  awk -v n="$n" 'BEGIN{
    print "\x27use strict\x27;";
    print "let total = 0;";
    for(i=0;i<n;i++){ printf "function f%d(x) { return (x * %d + %d) %% 97; }\n", i, i, i*3+1; }
    for(i=0;i<n;i++){ printf "total += f%d(%d);\n", i, i; }
    print "console.error(\x27total=\x27 + total);";
  }' > sea-bench.js
  cat > sea-config.json <<CFG
{
  "main": "sea-bench.js",
  "output": "sea-bench.blob",
  "disableExperimentalSEAWarning": true,
  "useCodeCache": false
}
CFG

  for label in main branch; do
    rm -f sea-bench.blob "sea-bench-$label"
    "$BIN_DIR/node-$label" --experimental-sea-config sea-config.json >/dev/null 2>&1
    if [ ! -f sea-bench.blob ]; then
      log "FATAL: n=$n $label did not produce sea-bench.blob"
      exit 1
    fi
    cp "$BIN_DIR/node-$label" "sea-bench-$label"
    if ! npx --yes postject "sea-bench-$label" NODE_SEA_BLOB sea-bench.blob \
        --sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2 >/dev/null 2>&1; then
      log "FATAL: postject failed for n=$n $label"
      exit 1
    fi
    chmod +x "sea-bench-$label"
  done

  main_result=$(run_series main)
  branch_result=$(run_series branch)
  main_mean=$(echo "$main_result" | awk '{print $2}')
  branch_mean=$(echo "$branch_result" | awk '{print $2}')
  pct=$(awk -v m="$main_mean" -v b="$branch_mean" 'BEGIN{printf "%.1f", (m-b)/m*100}')
  echo "$n $main_mean $branch_mean $pct" >> summary.txt
  log "n=$n main=${main_mean}ms branch=${branch_mean}ms  branch is ${pct}% faster"
done

echo
log "=== Summary (also in $RESULTS_DIR/summary.txt) ==="
column -t summary.txt
