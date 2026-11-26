#!/usr/bin/env bash
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Regenerate the garage fixture corpus from a local d8 build.
#
#   tools/gen-fixtures.sh /path/to/out/arm64.release/d8 [more/d8 ...]
#
# With no arguments it uses $GARAGE_D8, else falls back to the builds listed in
# DEFAULT_D8S below. Output lands in fixtures/<arch>-v<version>/ together with a
# manifest.json recording the exact command line behind every file.
#
# Two profiles are generated (see docs/printer-parser-contract.md):
#
#   clean  --no-concurrent-recompilation --predictable
#          --no-trace-with-compilation-id
#          Byte-for-byte reproducible: rerunning this script on the same d8
#          reproduces these files exactly, heap addresses included. Golden tests
#          should prefer these.
#
#   raw    No extra flags. Concurrent recompilation is on, trace lines carry
#          volatile [ML:<id>] compilation ids, and output from several threads
#          interleaves. NOT reproducible; this is what an engineer actually sees.
#
set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FIXTURE_ROOT="$REPO_ROOT/fixtures"

# Where generated builds land. Defaults to the corpus itself; the format-drift
# checker (tools/check-format-drift.sh) points it at a scratch directory so a
# fresh d8 can be measured without touching the checked-in fixtures. Workloads
# and the cwd stay under FIXTURE_ROOT either way, so the script paths recorded
# inside the traces do not move.
OUT_ROOT="${GARAGE_OUT_ROOT:-$FIXTURE_ROOT}"

DEFAULT_D8S=(
  "$HOME/repos/v8/v8/out/arm64.release/d8"        # tip of tree, arm64
  "$HOME/repos/v8/v8/out/arm64.release.baseline/d8" # older V8, same arch
  "$HOME/repos/v8/v8/out/x64.release/d8"          # same older V8, other arch
)

CLEAN_FLAGS=(--no-concurrent-recompilation --predictable --no-trace-with-compilation-id)

# One row per fixture: "<name>|<profile>|<workload>|<flags...>"
# Flags are space-separated; the profile flags are prepended for "clean".
FIXTURES=(
  # --- Maglev graphs: the primary input format ------------------------------
  "maglev-graphs.simple|clean|simple.js|--print-maglev-graphs"
  "maglev-graphs.truncation|clean|truncation.js|--print-maglev-graphs"
  "maglev-graphs.inlining|clean|inlining.js|--print-maglev-graphs"
  "maglev-graphs.poly|clean|poly.js|--print-maglev-graphs"
  "maglev-graphs.mixed|clean|mixed.js|--print-maglev-graphs"
  "maglev-graphs.throw|clean|throw.js|--print-maglev-graphs"
  "maglev-graph.simple|clean|simple.js|--print-maglev-graph"

  # --- Verbose deopt frames (spike-findings.md 12) --------------------------
  # --print-maglev-deopt-verbose swaps "(N live vars)" for the full register
  # map, changing the deopt sub-grammar. --trace-deopt-verbose *weakly implies*
  # it, so the flag correlation-keys.md recommends silently changes the graph
  # format too -- and adds "VOs : { … }" lines on top. Both are in the corpus
  # so the parser is tested against all three renderings.
  "maglev-graphs+deoptverbose.deopt|clean|deopt-eager.js|--print-maglev-graphs --print-maglev-deopt-verbose"
  "maglev-graphs+deoptverbose.throw|clean|throw.js|--print-maglev-graphs --print-maglev-deopt-verbose"
  "maglev-graphs+tracedeoptverbose.deopt|clean|deopt-eager.js|--print-maglev-graphs --trace-deopt-verbose"

  # --- Interleaved pass tracing (PLAN 6.1 annotation attachment) ------------
  # --trace-maglev-inlining is the exemplar: free-form lines printed *inside* a
  # compilation, between the begin banner and the graph.
  "maglev-graphs+inlining.inlining|clean|inlining.js|--print-maglev-graphs --trace-maglev-inlining"
  "maglev-graphs+graphbuilding.truncation|clean|truncation.js|--print-maglev-graphs --trace-maglev-graph-building"
  "maglev-graphs+regalloc.simple|clean|simple.js|--print-maglev-graphs --trace-maglev-regalloc"
  # Same thing with the volatile compilation ids left in, so the parser is
  # exercised against the "[ML:38564] " line prefix engineers actually see.
  "maglev-graphs+inlining-ids.inlining|raw-predictable|inlining.js|--print-maglev-graphs --trace-maglev-inlining"

  # --- Pipeline + code dump interleaved (TODO 8.9) --------------------------
  # The code dump prints right after the pipeline that produced it and merges
  # into that compilation as its last phases; needs v8_enable_disassembler.
  "maglev-graphs+code.simple|clean|simple.js|--print-maglev-graphs --print-opt-code"
  "maglev-graphs+maglev-code.simple|clean|simple.js|--print-maglev-graphs --print-maglev-code"

  # --- Tiering & lifecycle --------------------------------------------------
  "trace-opt+deopt.deopt|clean|deopt-eager.js|--trace-opt --trace-deopt"
  "trace-opt+deopt.mixed|clean|mixed.js|--trace-opt --trace-deopt"
  "trace-deopt-verbose.deopt|clean|deopt-eager.js|--trace-opt --trace-deopt-verbose"
  "trace-osr.osr|clean|osr.js|--trace-osr --trace-opt"
  "trace-gc.gc|clean|gc.js|--trace-gc"
  "trace-prototype-users.poly|clean|poly.js|--trace-prototype-users"

  # --- Bytecode, codegen, other pipelines -----------------------------------
  "print-bytecode.simple|clean|simple.js|--print-bytecode"
  "print-opt-code.simple|clean|simple.js|--print-opt-code"
  "turbolev-frontend.simple|clean|simple.js|--turbolev --print-turbolev-frontend"
  "turbo-graph.simple|clean|simple.js|--trace-turbo-graph"

  # --- The MVP invocation (PLAN 9) ------------------------------------------
  "mvp.deopt|clean|deopt-eager.js|--print-maglev-graphs --trace-deopt"
  "mvp.mixed|clean|mixed.js|--print-maglev-graphs --trace-deopt"

  # --- Realistic, non-reproducible: concurrent recompilation on -------------
  "concurrent.mixed|raw|mixed.js|--print-maglev-graphs --trace-opt --trace-deopt"
)

sha256() {
  if command -v shasum >/dev/null 2>&1; then shasum -a 256 "$1" | cut -d' ' -f1
  else sha256sum "$1" | cut -d' ' -f1; fi
}

json_escape() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }

gen_for_d8() {
  local d8="$1"
  if [[ ! -x "$d8" ]]; then echo "skip (not executable): $d8" >&2; return 1; fi

  local version arch out_dir build_dir v8_root git_hash
  version="$("$d8" -e 'print(version())' 2>/dev/null | tail -1 | sed 's/ (candidate)//')"
  arch="$(uname -m)"
  case "$(file -b "$d8")" in
    *x86_64*|*x86-64*) arch=x64 ;;
    *arm64*|*aarch64*) arch=arm64 ;;
  esac
  build_dir="$(dirname "$d8")"
  v8_root="$(cd "$build_dir/../.." 2>/dev/null && pwd || echo unknown)"
  git_hash="$(cd "$v8_root" 2>/dev/null && git rev-parse --short HEAD 2>/dev/null || echo unknown)"
  local d8_label="${d8#"$v8_root/"}"

  out_dir="$OUT_ROOT/$arch-v$version"
  mkdir -p "$out_dir"
  echo "==> $d8  ->  $out_dir" >&2

  local manifest="$out_dir/manifest.json"
  {
    printf '{\n'
    printf '  "d8": "%s",\n'          "$(json_escape "$d8_label")"
    printf '  "v8_version": "%s",\n'  "$(json_escape "$version")"
    printf '  "v8_git_hash": "%s",\n' "$(json_escape "$git_hash")"
    printf '  "arch": "%s",\n'        "$arch"
    printf '  "host": "%s",\n'        "$(uname -s)"
    printf '  "clean_profile_flags": "%s",\n' "${CLEAN_FLAGS[*]}"
    printf '  "fixtures": [\n'
  } > "$manifest"

  local first=1 row name profile workload flags_str
  for row in "${FIXTURES[@]}"; do
    IFS='|' read -r name profile workload flags_str <<< "$row"
    local -a flags=()
    read -r -a flags <<< "$flags_str"

    local -a full=()
    case "$profile" in
      clean)           full=("${CLEAN_FLAGS[@]}" "${flags[@]}") ;;
      raw-predictable) full=(--no-concurrent-recompilation --predictable "${flags[@]}") ;;
      raw)             full=("${flags[@]}") ;;
      *) echo "unknown profile: $profile" >&2; continue ;;
    esac

    local file="$out_dir/$name.log"
    # cwd = fixtures/ so the script path recorded inside the trace stays
    # relative and stable across machines.
    ( cd "$FIXTURE_ROOT" && "$d8" "${full[@]}" "workloads/$workload" ) > "$file" 2>&1
    local status=$?

    # Run a second time and compare: byte-reproducibility is a property golden
    # tests depend on, so record it instead of assuming it.
    local probe="$out_dir/.reproducibility-probe"
    ( cd "$FIXTURE_ROOT" && "$d8" "${full[@]}" "workloads/$workload" ) > "$probe" 2>&1
    local reproducible=true
    cmp -s "$file" "$probe" || reproducible=false
    rm -f "$probe"

    local bytes lines
    bytes=$(wc -c < "$file" | tr -d ' ')
    lines=$(wc -l < "$file" | tr -d ' ')
    printf '    %-42s %8s B  %6s lines  exit=%d  %s\n' \
      "$name.log" "$bytes" "$lines" "$status" \
      "$([[ $reproducible == true ]] && echo 'stable' || echo 'VOLATILE')" >&2

    [[ $first -eq 0 ]] && printf ',\n' >> "$manifest"
    first=0
    {
      printf '    {"file": "%s", "workload": "%s", "profile": "%s", ' \
        "$name.log" "workloads/$workload" "$profile"
      printf '"flags": "%s", "exit": %d, "bytes": %s, "lines": %s, ' \
        "$(json_escape "${full[*]}")" "$status" "$bytes" "$lines"
      printf '"reproducible": %s, "sha256": "%s"}' "$reproducible" "$(sha256 "$file")"
    } >> "$manifest"
  done

  printf '\n  ]\n}\n' >> "$manifest"
}

main() {
  local -a d8s=()
  if [[ $# -gt 0 ]]; then d8s=("$@")
  elif [[ -n "${GARAGE_D8:-}" ]]; then d8s=("$GARAGE_D8")
  else d8s=("${DEFAULT_D8S[@]}"); fi

  local any=0
  for d8 in "${d8s[@]}"; do gen_for_d8 "$d8" && any=1; done
  [[ $any -eq 1 ]] || { echo "no usable d8 build found" >&2; exit 1; }
}

main "$@"
