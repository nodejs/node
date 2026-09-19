#!/usr/bin/env bash
# TEMPORARY benchmarking helper — not part of this change, remove before merge.
# Compares the already-built main binary against this branch, rebuilt in place.
set -uo pipefail

REPO=/workspaces/node
BRANCH=eager-compile-embedder-main
BIN_DIR="$HOME/bench-binaries"
RESULTS_DIR="$HOME/bench-results"
MAKE_JOBS=4
FUNCTION_COUNT=50000
WARMUP_RUNS=3
TIMED_RUNS=25

log() { echo "[$(date '+%H:%M:%S')] $*"; }

mkdir -p "$BIN_DIR" "$RESULTS_DIR"
cd "$REPO"

# ---- 1. preserve whatever's currently built as "main" (only if not already saved) ----
if [ ! -f "$BIN_DIR/node-main" ]; then
  if [ ! -x out/Release/node ]; then
    log "FATAL: out/Release/node not found — expected main already built."
    exit 1
  fi
  cp out/Release/node "$BIN_DIR/node-main"
  log "Preserved main binary: $("$BIN_DIR/node-main" --version)"
else
  log "main binary already preserved: $("$BIN_DIR/node-main" --version)"
fi

# ---- 2. switch to the branch and rebuild (incremental) ----
git fetch origin "$BRANCH"
git checkout "$BRANCH"

if [ ! -f "$BIN_DIR/node-branch" ]; then
  log "Building branch (make -j$MAKE_JOBS) — should be incremental/fast, only node_contextify.cc differs"
  time make -j"$MAKE_JOBS" 2>&1 | tee "$RESULTS_DIR/build-branch.log"
  if [ ! -x out/Release/node ]; then
    log "FATAL: branch build did not produce out/Release/node. Check $RESULTS_DIR/build-branch.log. Re-run this script to resume."
    exit 1
  fi
  cp out/Release/node "$BIN_DIR/node-branch"
  log "Preserved branch binary: $("$BIN_DIR/node-branch" --version)"
else
  log "branch binary already preserved: $("$BIN_DIR/node-branch" --version)"
fi

# ---- 3. heavy fixture, self-reports resource usage ----
cd "$RESULTS_DIR"
awk -v n="$FUNCTION_COUNT" 'BEGIN{
  print "\x27use strict\x27;";
  print "let total = 0;";
  for(i=0;i<n;i++){
    printf "function f%d(x) { return (x * %d + %d) %% 97; }\n", i, i, i*3+1;
  }
  print "const start = process.hrtime.bigint();";
  for(i=0;i<n;i++){
    printf "total += f%d(%d);\n", i, i;
  }
  print "const end = process.hrtime.bigint();";
  print "const ru = process.resourceUsage();";
  print "console.error(\x27exec_ns=\x27 + (end - start).toString() + \x27 total=\x27 + total +";
  print "  \x27 userCPUms=\x27 + Math.round(ru.userCPUTime / 1000) +";
  print "  \x27 sysCPUms=\x27 + Math.round(ru.systemCPUTime / 1000) +";
  print "  \x27 maxRSSkb=\x27 + ru.maxRSS);";
}' > sea-bench.js
cat > sea-config.json <<'CFG'
{
  "main": "sea-bench.js",
  "output": "sea-bench.blob",
  "disableExperimentalSEAWarning": true,
  "useCodeCache": false
}
CFG

# ---- 4. build a SEA from each preserved binary, with CODE_CACHE debug logging ----
for label in main branch; do
  rm -f sea-bench.blob "sea-bench-$label"
  NODE_DEBUG_NATIVE=CODE_CACHE "$BIN_DIR/node-$label" --experimental-sea-config sea-config.json \
    > "sea-build-$label.log" 2>&1
  if [ ! -f sea-bench.blob ]; then
    log "FATAL: $label did not produce sea-bench.blob — check $RESULTS_DIR/sea-build-$label.log"
    exit 1
  fi
  cp "$BIN_DIR/node-$label" "sea-bench-$label"
  if ! npx --yes postject "sea-bench-$label" NODE_SEA_BLOB sea-bench.blob \
      --sentinel-fuse NODE_SEA_FUSE_fce680ab2cc467b6e072b8b5df1996b2 >"postject-$label.log" 2>&1; then
    log "FATAL: postject failed for $label — check $RESULTS_DIR/postject-$label.log"
    exit 1
  fi
  chmod +x "sea-bench-$label"
  if ! "./sea-bench-$label" 2>&1 >/dev/null | grep -q '^exec_ns='; then
    log "FATAL: sea-bench-$label doesn't behave like our SEA — injection likely broken."
    exit 1
  fi
done

log "=== Compile strategy actually used ==="
echo "--- main ---";   grep "Compiling" sea-build-main.log
echo "--- branch ---"; grep "Compiling" sea-build-branch.log
echo

# ---- 5. timed runs ----
run_series() {
  local label="$1"
  local bin="./sea-bench-$label"
  for i in $(seq 1 "$WARMUP_RUNS"); do "$bin" >/dev/null 2>/dev/null || true; done

  local -a samples=()
  local last_internal=""
  for i in $(seq 1 "$TIMED_RUNS"); do
    local start end ms
    start=$(date +%s%N)
    last_internal=$("$bin" 2>&1 >/dev/null)
    end=$(date +%s%N)
    ms=$(( (end - start) / 1000000 ))
    samples+=("$ms")
  done

  printf '%s\n' "${samples[@]}" | sort -n | awk -v label="$label" -v internal="$last_internal" '
    { sum += $1; a[NR] = $1 }
    END {
      n = NR
      mean = sum / n
      median = (n % 2 == 1) ? a[(n+1)/2] : (a[n/2] + a[n/2+1]) / 2
      sq = 0
      for (i = 1; i <= n; i++) sq += (a[i] - mean)^2
      stddev = sqrt(sq / n)
      printf "[%s] n=%d mean=%.1fms median=%.1fms min=%dms max=%dms stddev=%.1fms\n", \
        label, n, mean, median, a[1], a[n], stddev
      printf "[%s] last run internal metrics: %s\n", label, internal
    }'
}

log "=== Timing: $WARMUP_RUNS warmup + $TIMED_RUNS measured runs each ==="
run_series main
run_series branch
log "Done. Results/logs in $RESULTS_DIR, binaries in $BIN_DIR."
