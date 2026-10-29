#!/usr/bin/env vpython3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import annotations
import sys
import subprocess


def get_recent_commits(n: int = 5) -> None:
  try:
    # Get the last N commits
    git_log_cmd = ["git", "log", f"-n {n}", "--oneline", "--format=%H"]
    result = subprocess.run(
        git_log_cmd, capture_output=True, text=True, check=True)
    commit_hashes = result.stdout.strip().split("\n")

    if not commit_hashes or commit_hashes == [""]:
      print("No commits found.")
      return

    for commit in commit_hashes:
      commit = commit.strip()
      if not commit:
        continue

      print("=" * 80)
      print(f"COMMIT HASH: {commit}")

      # Get commit message
      msg_cmd = ["git", "show", "--format=%B", "-s", commit]
      msg_result = subprocess.run(
          msg_cmd, capture_output=True, text=True, check=True)
      print("COMMIT MESSAGE:")
      print(msg_result.stdout.strip())
      print("-" * 40)

      # Get diff
      diff_cmd = ["git", "show", "--format=", commit]
      diff_result = subprocess.run(
          diff_cmd, capture_output=True, text=True, check=True)
      print("DIFF:")
      print(diff_result.stdout.strip())
      print("=" * 80)
      print("\n")

  except subprocess.CalledProcessError as e:
    print(f"Error running git commands: {e}", file=sys.stderr)
    sys.exit(1)


if __name__ == "__main__":
  n = 5
  if len(sys.argv) > 1:
    try:
      n = int(sys.argv[1])
    except ValueError:
      print(f"Invalid argument: {sys.argv[1]}. Defaulting to 5 commits.")

  get_recent_commits(n)
