#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Retrieves branch diffs, uncommitted changes, or recent commits for doc sync."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys

# Mapping from src/ subdirectories to candidate docs/ directories or files.
SUBSYSTEM_DOCS_MAP: dict[str, list[str]] = {
    "src/ast": ["docs/parsing/"],
    "src/baseline": ["docs/compiler/sparkplug/", "docs/runtime/tiering.md"],
    "src/builtins": ["docs/builtins/", "docs/torque/"],
    "src/codegen": ["docs/codegen/"],
    "src/common": [
        "docs/heap/pointer-compression.md", "docs/heap/pointer-tagging.md"
    ],
    "src/compiler": [
        "docs/compiler/turbofan/", "docs/compiler/turboshaft/", "docs/compiler/"
    ],
    "src/deoptimizer": ["docs/runtime/deoptimization.md"],
    "src/execution": ["docs/runtime/"],
    "src/flags": ["docs/feature-launch-process.md"],
    "src/handles": ["docs/heap/handles.md"],
    "src/heap": ["docs/heap/"],
    "src/ic": ["docs/runtime/hidden-classes-and-ics.md"],
    "src/interpreter": [
        "docs/interpreter/", "docs/parsing/hole-checks-bytecode.md"
    ],
    "src/maglev": [
        "docs/compiler/maglev/", "docs/parsing/hole-checks-optimized.md"
    ],
    "src/objects": ["docs/objects/", "docs/heap/objects-and-maps.md"],
    "src/parsing": ["docs/parsing/"],
    "src/regexp": ["docs/regexp/"],
    "src/runtime": ["docs/runtime/"],
    "src/sandbox": ["docs/sandbox/"],
    "src/snapshot": ["docs/snapshot/"],
    "src/torque": ["docs/torque/"],
    "src/wasm": ["docs/wasm/"],
    "src/zone": ["docs/heap/zone-allocator.md"],
}


def _run_git(args: list[str],
             check: bool = True) -> subprocess.CompletedProcess[str]:
  return subprocess.run(
      ["git", "--no-pager", *args],
      capture_output=True,
      text=True,
      check=check,
  )


def _candidate_docs_for_files(modified_files: list[str]) -> list[str]:
  candidates: set[str] = set()
  for file_path in modified_files:
    normalized = file_path.strip()
    if not normalized:
      continue
    for prefix, doc_paths in SUBSYSTEM_DOCS_MAP.items():
      if normalized.startswith(prefix + "/") or normalized == prefix:
        candidates.update(doc_paths)
  return sorted(candidates)


def _print_candidate_docs(modified_files: list[str]) -> None:
  candidates = _candidate_docs_for_files(modified_files)
  if not candidates:
    return
  print("CANDIDATE DOCS DIRECTORIES/FILES TO CHECK:")
  for candidate in candidates:
    print(f"  - {candidate}")
  print("-" * 40)


def check_branch_diff(base_ref: str | None = None) -> None:
  """Prints the diff and commit messages for the current branch vs upstream."""
  if base_ref is None:
    upstream_res = _run_git(
        ["rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}"],
        check=False,
    )
    if upstream_res.returncode == 0 and upstream_res.stdout.strip():
      base_ref = upstream_res.stdout.strip()
    else:
      base_ref = "origin/main"

  print("=" * 80)
  print(f"BRANCH DIFF AGAINST: {base_ref}")

  log_res = _run_git(["log", "--oneline", f"{base_ref}..HEAD"], check=False)
  if log_res.returncode == 0 and log_res.stdout.strip():
    print("BRANCH COMMITS:")
    print(log_res.stdout.strip())
    print("-" * 40)

  files_res = _run_git(["diff", "--name-only", f"{base_ref}...HEAD"],
                       check=False)
  committed_files = [
      line for line in files_res.stdout.strip().splitlines() if line.strip()
  ]

  diff_res = _run_git(["diff", f"{base_ref}...HEAD"], check=False)
  committed_diff = diff_res.stdout.strip()

  uncommitted_files_res = _run_git(
      ["diff", "--name-only", "HEAD", "--", "src/", "include/"], check=False)
  uncommitted_files = [
      line for line in uncommitted_files_res.stdout.strip().splitlines()
      if line.strip()
  ]
  uncommitted_diff_res = _run_git(["diff", "HEAD", "--", "src/", "include/"],
                                  check=False)
  uncommitted_diff = uncommitted_diff_res.stdout.strip()

  modified_files = list(dict.fromkeys([*committed_files, *uncommitted_files]))
  diff_parts = [d for d in (committed_diff, uncommitted_diff) if d]
  diff_text = "\n".join(diff_parts)

  if uncommitted_diff:
    if committed_diff:
      print("NOTE: Including uncommitted changes in src/ and include/ "
            "alongside committed branch changes.")
    else:
      print("NOTE: No committed diff against upstream; showing uncommitted "
            "changes in src/ and include/.")
    print("-" * 40)

  if not diff_text:
    print("No branch or uncommitted src/ changes found.")
    print("=" * 80)
    return

  print("MODIFIED FILES:")
  for f in modified_files:
    print(f"  {f}")
  print("-" * 40)
  _print_candidate_docs(modified_files)
  print("DIFF:")
  print(diff_text)
  print("=" * 80)


def check_uncommitted_diff() -> None:
  """Prints uncommitted working-tree changes against HEAD."""
  print("=" * 80)
  print("UNCOMMITTED CHANGES (git diff HEAD)")
  files_res = _run_git(["diff", "--name-only", "HEAD"], check=True)
  modified_files = [
      line for line in files_res.stdout.strip().splitlines() if line.strip()
  ]
  diff_res = _run_git(["diff", "HEAD"], check=True)
  diff_text = diff_res.stdout.strip()
  if not diff_text:
    print("No uncommitted changes found.")
    print("=" * 80)
    return

  print("MODIFIED FILES:")
  for f in modified_files:
    print(f"  {f}")
  print("-" * 40)
  _print_candidate_docs(modified_files)
  print("DIFF:")
  print(diff_text)
  print("=" * 80)


def get_recent_commits(n: int = 5, rev_range: str | None = None) -> None:
  """Prints the last N commits (or commits in rev_range) with their diffs."""
  try:
    if rev_range:
      git_log_cmd = ["log", "--oneline", "--format=%H", rev_range]
    else:
      git_log_cmd = ["log", f"-n{n}", "--oneline", "--format=%H"]
    result = _run_git(git_log_cmd, check=True)
    commit_hashes = [
        c.strip() for c in result.stdout.strip().splitlines() if c.strip()
    ]

    if not commit_hashes:
      print("No commits found.")
      return

    for commit in commit_hashes:
      print("=" * 80)
      print(f"COMMIT HASH: {commit}")

      msg_result = _run_git(["show", "--format=%B", "-s", commit], check=True)
      print("COMMIT MESSAGE:")
      print(msg_result.stdout.strip())
      print("-" * 40)

      files_result = _run_git(["show", "--pretty=", "--name-only", commit],
                              check=True)
      modified_files = [
          line for line in files_result.stdout.strip().splitlines()
          if line.strip()
      ]
      _print_candidate_docs(modified_files)

      diff_result = _run_git(["show", "--format=", commit], check=True)
      print("DIFF:")
      print(diff_result.stdout.strip())
      print("=" * 80)
      print()

  except subprocess.CalledProcessError as e:
    print(f"Error running git commands: {e}", file=sys.stderr)
    sys.exit(1)


def main(argv: list[str] | None = None) -> int:
  parser = argparse.ArgumentParser(
      description=(
          "Retrieve branch diffs, uncommitted diffs, or recent commits to "
          "check for invalidated documentation in docs/."))
  parser.add_argument(
      "count",
      nargs="?",
      type=int,
      default=None,
      help="Number of recent commits to inspect (default: 5).",
  )
  parser.add_argument(
      "-b",
      "--branch",
      action="store_true",
      help=("Inspect changes on the current branch relative to @{upstream} "
            "(git diff @{upstream}...HEAD), falling back to uncommitted src/ "
            "changes if no branch commits exist."),
  )
  parser.add_argument(
      "--base",
      type=str,
      default=None,
      help="Explicit base ref to diff against with --branch (default: @{upstream}).",
  )
  parser.add_argument(
      "-u",
      "--uncommitted",
      action="store_true",
      help="Inspect uncommitted working-tree changes (git diff HEAD).",
  )
  parser.add_argument(
      "-r",
      "--range",
      dest="rev_range",
      type=str,
      default=None,
      help="Inspect commits in a specific git revision range (e.g. origin/main..HEAD).",
  )

  # Preserve legacy behavior if an invalid positional argument is passed.
  args, unknown = parser.parse_known_args(argv)
  if unknown:
    print(f"Invalid argument: {' '.join(unknown)}. Defaulting to 5 commits.")
    get_recent_commits(5)
    return 0

  if args.branch or args.base:
    check_branch_diff(args.base)
  elif args.uncommitted:
    check_uncommitted_diff()
  elif args.rev_range:
    get_recent_commits(rev_range=args.rev_range)
  else:
    get_recent_commits(args.count if args.count is not None else 5)
  return 0


if __name__ == "__main__":
  sys.exit(main())
