#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Any
import argparse
import subprocess
import sys

tools_dir = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(tools_dir))

import find_depot_tools

find_depot_tools.add_depot_tools_to_path()
import gclient_eval


@dataclass(frozen=True)
class Dependency:
  """A DEPS dependency and its gitignore rule."""
  path: str
  gitignore_rule: str | None


def print_err(*args: Any, **kwargs: Any) -> None:
  print(*args, file=sys.stderr, **kwargs)


def parse_deps(repo_root: Path) -> list[str]:
  deps_path = repo_root / "DEPS"
  content = deps_path.read_text(encoding="utf-8")
  data = gclient_eval.Parse(content, str(deps_path))
  dep_paths = set(data.get("deps", {}).keys())
  return sorted(dep_paths)


def get_git_ignore_rules(paths: list[str], repo_root: Path) -> list[Dependency]:
  stdout: str = subprocess.run(["git", "check-ignore", "-v", "-n", *paths],
                               text=True,
                               cwd=repo_root,
                               capture_output=True,
                               check=False).stdout
  checked: list[Dependency] = []
  for line in stdout.splitlines():
    if not line:
      continue
    # Example git check-ignore outputs:
    # .gitignore:81:/third_party/*    third_party/perfetto
    # ::      src/maglev
    gitignore_rule, path = line.split("\t", maxsplit=1)
    if gitignore_rule == "::":
      gitignore_rule = None
    checked.append(Dependency(path, gitignore_rule))
  return checked


def verify_deps(dep_paths: list[str], verbose: bool, repo_root: Path) -> int:
  all_deps = get_git_ignore_rules(dep_paths, repo_root)
  missing_gitignore_deps = [dep for dep in all_deps if not dep.gitignore_rule]

  if verbose:
    print(f'{"DEPS Path":<40} {"Ignore Rule":<40}')
    print("-" * 80)
    for dep in all_deps:
      rule_str = dep.gitignore_rule or "MISSING IGNORED"
      print(f"{dep.path:<40} {rule_str:<40}")

  if not missing_gitignore_deps:
    return 0

  if not verbose:
    # verbose already printed "MISSING IGNORED" above
    print_err("Error: The following dependencies in DEPS are NOT gitignored:")
    for dep in sorted(missing_gitignore_deps, key=lambda x: x.path):
      print_err(f"  {dep.path}")
    print_err("Add the corresponding deps-paths to .gitignore")
  return 1


def main() -> int:
  parser = argparse.ArgumentParser(description="Verify DEPS are gitignored.")
  parser.add_argument(
      "-v",
      "--verbose",
      action="store_true",
      help="List all DEPS paths and their matching ignore rules")
  args = parser.parse_args()

  repo_root: Path = tools_dir.parent
  dep_paths: list[str] = parse_deps(repo_root)
  return verify_deps(dep_paths, args.verbose, repo_root)


if __name__ == "__main__":
  sys.exit(main())
