#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Delta-debugging (ddmin) CLI for minimizing V8 reproducer flags and JS lines."""

from __future__ import annotations

import argparse
import dataclasses
import json
import os
import re
import shlex
import subprocess
import sys
import tempfile
import time
from typing import Callable, List, Optional, Sequence, Tuple


@dataclasses.dataclass
class RunResult:
  reproduces: bool
  returncode: Optional[int]
  timed_out: bool
  output: str


@dataclasses.dataclass
class MinimizeStats:
  runs: int = 0
  initial_flags: List[str] = dataclasses.field(default_factory=list)
  minimal_flags: List[str] = dataclasses.field(default_factory=list)
  initial_lines: int = 0
  minimal_lines: int = 0
  elapsed_seconds: float = 0.0


def build_command(cmd_template: str, flags: Sequence[str],
                  file_path: str) -> List[str]:
  """Builds argv from a command template, flags, and file path."""
  tokens = shlex.split(cmd_template)
  argv: List[str] = []
  saw_flags = False
  saw_file = False
  for token in tokens:
    if token == "{flags}":
      argv.extend(flags)
      saw_flags = True
    elif "{flags}" in token:
      argv.append(token.replace("{flags}", " ".join(flags)))
      saw_flags = True
    elif token == "{file}":
      if not saw_flags and flags:
        argv.extend(flags)
        saw_flags = True
      argv.append(file_path)
      saw_file = True
    elif "{file}" in token:
      if not saw_flags and flags:
        argv.extend(flags)
        saw_flags = True
      argv.append(token.replace("{file}", file_path))
      saw_file = True
    else:
      argv.append(token)
  if not saw_flags and flags:
    argv.extend(flags)
  if not saw_file:
    argv.append(file_path)
  return argv


def check_reproduces(
    cmd_template: str,
    flags: Sequence[str],
    content: str,
    suffix: str,
    timeout: float,
    match_re: Optional[re.Pattern[str]],
) -> RunResult:
  """Writes content to a temp file and checks whether the failure reproduces."""
  tmp_fd, tmp_path = tempfile.mkstemp(suffix=suffix, prefix="ddmin_")
  try:
    with os.fdopen(tmp_fd, "w", encoding="utf-8") as f:
      f.write(content)
    argv = build_command(cmd_template, flags, tmp_path)
    try:
      proc = subprocess.run(
          argv,
          stdout=subprocess.PIPE,
          stderr=subprocess.STDOUT,
          text=True,
          errors="replace",
          timeout=timeout,
      )
      output = proc.stdout or ""
      if match_re is not None:
        reproduces = bool(match_re.search(output))
      else:
        reproduces = proc.returncode != 0
      return RunResult(
          reproduces=reproduces,
          returncode=proc.returncode,
          timed_out=False,
          output=output,
      )
    except subprocess.TimeoutExpired as exc:
      out = ""
      if exc.stdout:
        out = (
            exc.stdout.decode("utf-8", errors="replace") if isinstance(
                exc.stdout, bytes) else str(exc.stdout))
      return RunResult(
          reproduces=False,
          returncode=None,
          timed_out=True,
          output=out,
      )
  finally:
    if os.path.exists(tmp_path):
      os.remove(tmp_path)


def ddmin(
    items: Sequence[str],
    test_fn: Callable[[Sequence[str]], bool],
    on_progress: Optional[Callable[[Sequence[str]], None]] = None,
) -> List[str]:
  """Zeller's delta-debugging algorithm returning a 1-minimal subset."""
  current = list(items)
  if not current:
    return current
  if test_fn([]):
    if on_progress:
      on_progress([])
    return []

  n = 2
  while len(current) >= 2:
    chunk_size = max(1, (len(current) + n - 1) // n)
    subsets = [
        current[i:i + chunk_size] for i in range(0, len(current), chunk_size)
    ]
    if len(subsets) == 1:
      break

    reduced = False
    # First try keeping each subset alone (fast large reductions).
    for sub in subsets:
      if len(sub) == len(current):
        continue
      if test_fn(sub):
        current = list(sub)
        if on_progress:
          on_progress(current)
        n = 2
        reduced = True
        break
    if reduced:
      continue

    # Next try removing each subset (keeping its complement).
    for i in range(len(subsets)):
      start = i * chunk_size
      end = min(len(current), start + chunk_size)
      complement = current[:start] + current[end:]
      if not complement:
        continue
      if test_fn(complement):
        current = complement
        if on_progress:
          on_progress(current)
        n = max(2, len(subsets) - 1)
        reduced = True
        break

    if not reduced:
      if n >= len(current):
        break
      n = min(len(current), n * 2)

  return current


def write_output(path: str, lines: Sequence[str]) -> None:
  """Writes minimized lines atomically-enough to the output path."""
  parent = os.path.dirname(os.path.abspath(path))
  if parent:
    os.makedirs(parent, exist_ok=True)
  with open(path, "w", encoding="utf-8") as f:
    f.writelines(lines)


def minimize_reproducer(
    cmd_template: str,
    input_path: str,
    output_path: str,
    flags: Sequence[str],
    timeout: float,
    match_pattern: Optional[str],
    minimize_flags: bool = True,
    minimize_lines: bool = True,
) -> Tuple[List[str], List[str], MinimizeStats]:
  """Minimizes flags and JS lines, saving the best reproducer incrementally."""
  start = time.monotonic()
  with open(input_path, "r", encoding="utf-8") as f:
    lines = f.readlines()

  suffix = os.path.splitext(input_path)[1] or ".js"
  match_re = re.compile(match_pattern, re.MULTILINE) if match_pattern else None
  stats = MinimizeStats(
      initial_flags=list(flags),
      minimal_flags=list(flags),
      initial_lines=len(lines),
      minimal_lines=len(lines),
  )

  def run_candidate(cand_flags: Sequence[str],
                    cand_lines: Sequence[str]) -> RunResult:
    stats.runs += 1
    return check_reproduces(
        cmd_template=cmd_template,
        flags=cand_flags,
        content="".join(cand_lines),
        suffix=suffix,
        timeout=timeout,
        match_re=match_re,
    )

  baseline = run_candidate(flags, lines)
  if not baseline.reproduces:
    raise RuntimeError(
        "Baseline input failed to reproduce target signature "
        f"(returncode={baseline.returncode}, timed_out={baseline.timed_out}).\n"
        f"Output:\n{baseline.output}")

  best_flags = list(flags)
  best_lines = list(lines)
  write_output(output_path, best_lines)

  if minimize_flags and best_flags:
    best_flags = ddmin(
        best_flags,
        lambda cand: run_candidate(cand, best_lines).reproduces,
    )
    stats.minimal_flags = list(best_flags)

  if minimize_lines and best_lines:
    # Fast pre-pass: strip blank and pure //-comment lines if not needed.
    stripped = [
        line for line in best_lines
        if line.strip() and not line.lstrip().startswith("//")
    ]
    if 0 < len(stripped) < len(best_lines):
      if run_candidate(best_flags, stripped).reproduces:
        best_lines = stripped
        write_output(output_path, best_lines)

    def save_lines(cand: Sequence[str]) -> None:
      write_output(output_path, cand)

    best_lines = ddmin(
        best_lines,
        lambda cand: run_candidate(best_flags, cand).reproduces,
        on_progress=save_lines,
    )
    write_output(output_path, best_lines)
    stats.minimal_lines = len(best_lines)

  stats.elapsed_seconds = round(time.monotonic() - start, 3)
  return best_flags, best_lines, stats


def _normalize_option_args(
    argv: Sequence[str],
    option_names: Sequence[str] = ("--flags", "--match"),
) -> List[str]:
  """Packs `--opt <val>` into `--opt=<val>` so flag-like values are accepted."""
  normalized: List[str] = []
  i = 0
  while i < len(argv):
    tok = argv[i]
    if tok in option_names and i + 1 < len(argv):
      normalized.append(f"{tok}={argv[i + 1]}")
      i += 2
    else:
      normalized.append(tok)
      i += 1
  return normalized


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description="Delta-debug V8 CLI flags and JS reproducer lines.")
  parser.add_argument(
      "--cmd",
      required=True,
      help="Command template (e.g. 'out/x64.debug/d8 {flags} {file}').",
  )
  parser.add_argument(
      "--file",
      required=True,
      help="Path to the input reproducer file.",
  )
  parser.add_argument(
      "--out",
      required=True,
      help="Path to write the minimized reproducer file.",
  )
  parser.add_argument(
      "--flags",
      default="",
      help="Space-separated candidate flags to minimize before line ddmin.",
  )
  parser.add_argument(
      "--match",
      default=None,
      help=(
          "Regex matched against combined stdout/stderr to confirm the exact "
          "failure signature (recommended to avoid syntax-error false hits)."),
  )
  parser.add_argument(
      "--timeout",
      type=float,
      default=10.0,
      help="Per-run timeout in seconds (default: 10.0).",
  )
  parser.add_argument(
      "--no-minimize-flags",
      action="store_true",
      help="Skip flag minimization.",
  )
  parser.add_argument(
      "--no-minimize-lines",
      action="store_true",
      help="Skip line minimization.",
  )
  parser.add_argument(
      "--json",
      action="store_true",
      help="Emit summary as JSON.",
  )
  raw_argv = sys.argv[1:] if argv is None else argv
  return parser.parse_args(_normalize_option_args(raw_argv))


def main(argv: Optional[Sequence[str]] = None) -> int:
  args = parse_args(argv)
  flags = shlex.split(args.flags) if args.flags else []
  try:
    min_flags, _, stats = minimize_reproducer(
        cmd_template=args.cmd,
        input_path=args.file,
        output_path=args.out,
        flags=flags,
        timeout=args.timeout,
        match_pattern=args.match,
        minimize_flags=not args.no_minimize_flags,
        minimize_lines=not args.no_minimize_lines,
    )
  except RuntimeError as exc:
    print(f"Error: {exc}", file=sys.stderr)
    return 1

  if args.json:
    print(json.dumps(dataclasses.asdict(stats), indent=2))
  else:
    print(f"Minimized {args.file} -> {args.out}: "
          f"{stats.initial_lines} -> {stats.minimal_lines} lines, "
          f"flags: {' '.join(min_flags) or '(none)'} "
          f"({stats.runs} runs in {stats.elapsed_seconds:.2f}s)")
  return 0


if __name__ == "__main__":
  sys.exit(main())
