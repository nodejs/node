#!/usr/bin/env bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Format-drift check against a fresh d8 (TODO 10.2).
#
#   tools/check-format-drift.sh [/path/to/d8 ...]
#
# With no arguments it uses $GARAGE_D8, else the builds gen-fixtures.sh lists.
#
# Run this by hand after syncing V8 — it is deliberately not a scheduled job.
# Everything it needs is a d8 built with the gates from README ("Getting a
# useful trace"): V8_ENABLE_MAGLEV_GRAPH_PRINTER and v8_enable_disassembler.
# Prebuilt/official V8 binaries have both off and will produce a corpus that
# reports drift everywhere, which is why this cannot run on stock CI.
#
# What it does:
#   1. regenerates the full fixture matrix from that d8 into a scratch dir,
#      leaving the checked-in corpus untouched;
#   2. runs the format_drift_report test over it.
#
# What counts as drift (tests/golden.rs, `Drift`): a phase banner no version in
# assets/markers.toml knows, a file whose sections stop tiling it, a graph flag
# that yields no compilation at all. New *content* is not drift — a new V8 is
# allowed to print new things; the parser losing ground is the finding.
#
# Exit status: 0 clean, 1 drift found, 2 could not run.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SCRATCH="${GARAGE_DRIFT_DIR:-${TMPDIR:-/tmp}/garage-drift-$$}"
KEEP="${GARAGE_DRIFT_KEEP:-0}"

cleanup() {
  if [[ "$KEEP" == "1" ]]; then
    echo "corpus kept at $SCRATCH" >&2
  else
    rm -rf "$SCRATCH"
  fi
}
trap cleanup EXIT

command -v cargo >/dev/null 2>&1 || {
  echo "cargo not found" >&2
  exit 2
}

mkdir -p "$SCRATCH" || exit 2

echo "==> regenerating the fixture matrix from a fresh d8"
echo "    scratch: $SCRATCH"
echo "    (the checked-in corpus under fixtures/ is not touched)"
echo

GARAGE_OUT_ROOT="$SCRATCH" "$REPO_ROOT/tools/gen-fixtures.sh" "$@" || {
  echo >&2
  echo "could not generate a corpus — is d8 built, and built with" >&2
  echo "V8_ENABLE_MAGLEV_GRAPH_PRINTER and v8_enable_disassembler?" >&2
  exit 2
}

# gen-fixtures.sh names each build directory after the version it measured, so
# discover rather than assume: the point of this script is that the version is
# new and unknown ahead of time.
# `mapfile` is bash 4; macOS still ships 3.2. Read the loop instead.
builds=()
while IFS= read -r dir; do
  builds+=("$dir")
done < <(find "$SCRATCH" -mindepth 1 -maxdepth 1 -type d | sort)

if [[ ${#builds[@]} -eq 0 ]]; then
  echo "no build directories produced under $SCRATCH" >&2
  exit 2
fi

status=0
for build in "${builds[@]}"; do
  echo
  echo "==> drift report for $(basename "$build")"
  # Run once, into a file: piping straight to sed would replace the test's exit
  # status with sed's, and that status is the whole answer.
  report="$SCRATCH/report-$(basename "$build").txt"
  GARAGE_DRIFT_CORPUS="$build" cargo test --manifest-path "$REPO_ROOT/Cargo.toml" \
    --test golden format_drift_report -- --ignored --nocapture >"$report" 2>&1
  rc=$?
  sed -n '/format-drift report/,/^test /p' "$report" | sed '$d'
  [[ $rc -ne 0 ]] && status=1
done

echo
if [[ $status -ne 0 ]]; then
  cat >&2 <<'EOF'
drift found.

If this is simply a newer V8, the fix is usually to add its phase names to
assets/markers.toml — the header there says where the table comes from
(src/maglev/maglev-phase.h, PhaseName()). Unknown banners still open phases,
so garage keeps working meanwhile; they are just marked unrecognised.

A PARTITION or EMPTY finding is different in kind: something the indexer
anchors on stopped matching, and that needs a parser change, not a table entry.
EOF
  exit 1
fi

echo "no drift against $(basename "${builds[0]}")"
