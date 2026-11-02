#!/bin/bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Runs the checks that every change to experimental/redline must pass before
# upload, and prints one line for each check. Run it from the root of the V8
# checkout, after you commit the change.
#
# Usage: check.sh [--out=DIR] [--d8=PATH] [--base=REV]
#   --out=DIR   the Redline build directory (default: out/redline)
#   --d8=PATH   the fuzzing build of d8 (default: out/fuzzbuild/d8)
#   --base=REV  the revision before the change (default: the merge base with
#               the upstream branch, or origin/main)
#
# Exit status: 0 if no check fails, 1 if a check fails.

set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT=out/redline
D8=out/fuzzbuild/d8
BASE=""
for arg in "$@"; do
  case "$arg" in
    --out=*) OUT="${arg#--out=}" ;;
    --d8=*) D8="${arg#--d8=}" ;;
    --base=*) BASE="${arg#--base=}" ;;
    *) echo "unknown argument: $arg" >&2; exit 2 ;;
  esac
done
if [ ! -d experimental/redline ]; then
  echo "Run this script from the root of the V8 checkout." >&2
  exit 2
fi
if [ -z "$BASE" ]; then
  BASE="$(git merge-base HEAD '@{upstream}' 2>/dev/null ||
          git merge-base HEAD origin/main)" || exit 2
fi
LOGS="$(mktemp -d /tmp/redline-check.XXXXXX)"
FAILED=0
RESULTS=()

record() {  # record NAME PASS|FAIL|SKIP|INFO DETAIL
  RESULTS+=("$(printf '%-4s  %-24s %s' "$2" "$1" "$3")")
  if [ "$2" = FAIL ]; then FAILED=1; fi
}

run() {  # run NAME COMMAND... : PASS if the command exits with 0
  local name="$1" log
  shift
  log="$LOGS/$(echo "$name" | tr -c 'a-z0-9' '_').log"
  if "$@" > "$log" 2>&1; then
    record "$name" PASS "$(tail -n 1 "$log" | cut -c1-90)"
  else
    record "$name" FAIL "see $log"
  fi
}

echo "Base revision: $(git log -1 --format='%h %s' "$BASE")"
echo "Logs: $LOGS"

if git cl format --dry-run > "$LOGS/format.log" 2>&1; then
  record "git cl format" PASS "no change needed"
else
  record "git cl format" FAIL "run 'git cl format' (see $LOGS/format.log)"
fi

if git cl lint > "$LOGS/lint.log" 2>&1; then
  record "git cl lint" PASS "0 errors"
else
  LINT_ERRORS="$(sed -n -E 's/^(Total errors found: |cpplint: )([0-9]+).*/\2/p' \
                 "$LOGS/lint.log" | tail -n 1)"
  record "git cl lint" FAIL \
    "${LINT_ERRORS:-?} C++ errors or Python errors (see $LOGS/lint.log)"
fi

if autoninja -C "$OUT" redline > "$LOGS/build.log" 2>&1; then
  record "build" PASS "autoninja -C $OUT redline"
  run "selftest --offline" "$OUT/redline" selftest --offline
  if [ -x "$D8" ]; then
    run "selftest --d8" "$OUT/redline" selftest --d8="$D8"
  else
    record "selftest --d8" FAIL "no d8 at $D8 (TUTORIAL.md section 1)"
  fi
  run "roundtrip --no-run" "$OUT/redline" roundtrip --dir=test/mjsunit --no-run
else
  record "build" FAIL "see $LOGS/build.log"
  record "selftests, roundtrip" SKIP "the build failed"
fi

TRACES="$(git diff --name-only "$BASE" -- experimental/redline/testdata | wc -l)"
if [ "$TRACES" = 0 ]; then
  record "golden traces" INFO "unchanged"
else
  record "golden traces" INFO "$TRACES changed: explain them in the CL description"
fi

mapfile -t MARKDOWN < <(git diff --name-only --diff-filter=AMR "$BASE" -- \
                        'experimental/redline/*.md')
if [ "${#MARKDOWN[@]}" = 0 ]; then
  record "markdown links" SKIP "no Markdown file changed"
else
  run "markdown links" vpython3 tools/dev/markdown_checker.py "${MARKDOWN[@]}"
fi

python3 "$SCRIPT_DIR/ste_check.py" --base="$BASE" > "$LOGS/ste.log" 2>&1
case $? in
  0) record "STE language" PASS "see $LOGS/ste.log" ;;
  3) record "STE language" SKIP "the linter is not available (see $LOGS/ste.log)" ;;
  *) record "STE language" FAIL "see $LOGS/ste.log" ;;
esac

echo
printf '%s\n' "${RESULTS[@]}"
exit "$FAILED"
