#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Starts one arm of a fair comparison. All arms get the same timeout and the
# same number of workers. The Redline arms get the same d8. The fuzzilli arm
# needs a d8 of the same revision built with v8_fuzzilli=true: fuzzilli/d8 of
# a bundle, or the d8 that TARGET names. The d8 flags come from the preset
# file in PRESETS, so all arms with the same PRESETS get the same flags.
#
# Usage: run_arm.sh ARM TARGET OUT DURATION_SEC [SEED]
#   ARM     redline-normal, redline-change or fuzzilli
#   TARGET  a bug bundle (see the redline-bug-reinjection skill), or a d8
#           binary for the coverage setup. redline-change needs a bundle.
#   OUT     the output directory. It must not exist.
#   SEED    the random seed of Redline (default 1)
# Environment:
#   REDLINE        the redline binary (redline arms)
#   FUZZILLI       the FuzzilliCli binary (fuzzilli arm)
#   FUZZILLI_ARGS  the options of Fuzzilli
#                  (default: --corpus=rarity --workerDelay=0)
#   PRESET         the Redline preset (default: TARGET/preset, else maglev)
#   PRESETS        the presets directory (default: the presets of this
#                  checkout)
#   EXTRA_FLAGS    more d8 flags for all arms, after the flags of the preset
#   JOBS           workers (default 5)
#   TIMEOUT_MS     timeout of one program (default 400)
#   SEEDS          a directory of seed files for Redline (default: no seeds)
#   RANDOM_FLAGS   1 keeps the random d8 flags of the Redline preset and gives
#                  Fuzzilli --argumentRandomization (default 0: all arms get
#                  the same fixed flags). The preset must set
#                  randomize_d8_flags.
#   OVERWRITE      1 removes an existing OUT first (default 0: stop)
#
# The script writes the setup and the command to OUT.start. It writes the exit
# status and the end time to OUT.end.
set -euo pipefail

if [ $# -lt 4 ]; then
  awk 'NR >= 6 && /^#/ { sub(/^# ?/, ""); print; next } NR >= 6 { exit }' "$0"
  exit 2
fi
ARM=$1 TARGET=$2 OUT=$3 DURATION=$4 SEED=${5:-1}
JOBS=${JOBS:-5}
TIMEOUT_MS=${TIMEOUT_MS:-400}
HERE=$(cd "$(dirname "$0")" && pwd)

if [ -d "$TARGET" ]; then
  BUNDLE=$TARGET
  BITS=$BUNDLE/bits/d8
  COUNTERS=$BUNDLE/counters/d8
  FUZZILLI_D8=$BUNDLE/fuzzilli/d8
  DEFAULT_PRESET=maglev
  [ -f "$BUNDLE/preset" ] && DEFAULT_PRESET=$(cat "$BUNDLE/preset")
else
  BUNDLE=""
  BITS=$TARGET
  COUNTERS=""
  FUZZILLI_D8=$TARGET
  DEFAULT_PRESET=maglev
fi
[ -x "$BITS" ] || { echo "no d8 at $BITS" >&2; exit 1; }
PRESET=${PRESET:-$DEFAULT_PRESET}

# One presets directory for all arms gives all arms the same d8 flags. The
# other settings of a preset come from the presets that each binary reads.
PRESETS=${PRESETS:-$HERE/../../../presets}
PRESET_FILE=$PRESETS/$PRESET.json
[ -f "$PRESET_FILE" ] || { echo "no preset file $PRESET_FILE" >&2; exit 1; }
if [ "${RANDOM_FLAGS:-0}" = 1 ] && ! python3 -c 'import json, sys; sys.exit(0 if json.load(open(sys.argv[1])).get("randomize_d8_flags") else 1)' "$PRESET_FILE"; then
  echo "RANDOM_FLAGS=1 needs a preset with randomize_d8_flags, for example maglev" >&2
  exit 1
fi

# The base flags of the V8 profile of Fuzzilli. Redline uses the same list.
BASE="--expose-gc --expose-externalize-string --omit-quit --allow-natives-syntax --fuzzing --harmony --experimental-fuzzing --js-staging --wasm-staging --wasm-fast-api --expose-fast-api --wasm-test-streaming"
# The flags of the preset and EXTRA_FLAGS, for every arm. An old Redline binary
# drops the flags of the preset when it gets --d8-args. Thus each Redline arm
# gets them again with --d8-extra-args. Two copies of a flag do no harm.
EXTRA=$(python3 -c 'import json, sys; print(" ".join(json.load(open(sys.argv[1])).get("d8_extra_args", [])))' "$PRESET_FILE")
if [ -n "${EXTRA_FLAGS:-}" ]; then
  EXTRA="${EXTRA:+$EXTRA }$EXTRA_FLAGS"
fi

D8_USED=$BITS
case "$ARM" in
  redline-normal|redline-change)
    [ -x "${REDLINE:-}" ] || { echo "set REDLINE to the redline binary" >&2; exit 1; }
    TOOL=$REDLINE
    COMMON=(--d8-args="$BASE" --preset="$PRESET" --timeout-ms="$TIMEOUT_MS"
            --max-execs-before-respawn=1000 --jobs="$JOBS" --seed="$SEED"
            --prelude=none --stats-interval=60 --save-corpus
            --duration="$DURATION" --out="$OUT")
    if [ "${RANDOM_FLAGS:-0}" != 1 ]; then
      COMMON+=(--config='{"randomize_d8_flags": false}')
    fi
    if [ -n "${SEEDS:-}" ]; then
      COMMON+=(--import="$SEEDS")
    fi
    if [ "$ARM" = redline-normal ]; then
      CMD=("$REDLINE" fuzz --d8="$BITS" "${COMMON[@]}" --d8-extra-args="$EXTRA")
    else
      [ -n "$BUNDLE" ] && [ -x "$COUNTERS" ] && [ -f "$BUNDLE/targets.json" ] || {
        echo "redline-change needs a bundle with counters/d8 and targets.json" >&2
        exit 1
      }
      D8_USED=$COUNTERS
      CMD=("$REDLINE" fuzz --d8="$COUNTERS" "${COMMON[@]}"
           --d8-extra-args="--fuzzilli-enable-optimized-code-coverage $EXTRA"
           --targets="$BUNDLE/targets.json")
    fi ;;
  fuzzilli)
    [ -x "${FUZZILLI:-}" ] || { echo "set FUZZILLI to the FuzzilliCli binary" >&2; exit 1; }
    if [ -n "${SEEDS:-}" ]; then
      echo "Fuzzilli cannot import JavaScript seed files directly. Run all arms without seeds." >&2
      exit 1
    fi
    [ -x "$FUZZILLI_D8" ] || { echo "no d8 of Fuzzilli at $FUZZILLI_D8" >&2; exit 1; }
    D8_USED=$FUZZILLI_D8
    TOOL=$FUZZILLI
    read -r -a FZ_ARGS <<< "${FUZZILLI_ARGS:---corpus=rarity --workerDelay=0}"
    CMD=("$FUZZILLI" --profile=v8 --storagePath="$OUT" "${FZ_ARGS[@]}"
         --exportStatistics --statisticsExportInterval=1
         --tag="$(basename "$OUT")" --jobs="$JOBS" --timeout="$TIMEOUT_MS"
         --maxRuntime="${DURATION}s")
    if [ -n "$EXTRA" ]; then
      CMD+=(--additionalArguments="$(echo "$EXTRA" | tr ' ' ',')")
    fi
    if [ "${RANDOM_FLAGS:-0}" = 1 ]; then
      CMD+=(--argumentRandomization)
    fi
    CMD+=("$FUZZILLI_D8") ;;
  *)
    echo "unknown arm $ARM (redline-normal, redline-change or fuzzilli)" >&2
    exit 2 ;;
esac

if [ -e "$OUT" ]; then
  if [ "${OVERWRITE:-0}" != 1 ]; then
    echo "$OUT exists. Archive it, or set OVERWRITE=1 to remove it." >&2
    exit 1
  fi
  rm -rf "$OUT"
fi
mkdir -p "$(dirname "$OUT")"
rm -f "$OUT.end"
{
  date -Is
  echo "arm=$ARM target=$TARGET preset=$PRESET jobs=$JOBS timeout=$TIMEOUT_MS seed=$SEED random_flags=${RANDOM_FLAGS:-0} seeds=${SEEDS:-none}"
  echo "extra=$EXTRA"
  echo "sha256 tool=$(sha256sum "$TOOL" | cut -c1-16) d8=$(sha256sum "$D8_USED" | cut -c1-16)"
  COMMIT_FILE=$(dirname "$TOOL")/../../COMMIT
  if [ -f "$COMMIT_FILE" ]; then echo "commit=$(cat "$COMMIT_FILE")"; fi
  echo "command: $(printf '%q ' "${CMD[@]}")"
} > "$OUT.start"

# Many crashing d8 processes can fill the disk with core files.
ulimit -c 0
set +e
"${CMD[@]}"
RC=$?
set -e
echo "rc=$RC end=$(date -Is)" > "$OUT.end"
exit "$RC"
