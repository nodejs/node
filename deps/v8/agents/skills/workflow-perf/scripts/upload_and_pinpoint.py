#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import annotations

import argparse
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys


def run_command(cmd: list[str], cwd: str | None = None) -> str:
  print(f"Running: {shlex.join(cmd)}")
  env = os.environ.copy()
  env["SKIP_GCE_AUTH_FOR_GIT"] = "1"
  env["GIT_EDITOR"] = "true"
  result = subprocess.run(
      cmd, cwd=cwd, env=env, capture_output=True, text=True, check=False)
  if result.returncode != 0:
    print(f"Error running command: {result.stderr}")
    print(f"Stdout: {result.stdout}")
    sys.exit(result.returncode)
  return result.stdout


def main() -> None:
  parser = argparse.ArgumentParser(
      description="Upload CL and start Pinpoint job.")
  parser.add_argument(
      "--benchmark",
      required=True,
      help="Crossbench benchmark name (e.g., jetstream3.0.crossbench)")
  parser.add_argument(
      "--bot", required=True, help="Pinpoint bot name (e.g., linux-r350-perf)")
  parser.add_argument(
      "--message", required=True, help="Commit message for git cl upload")

  home = Path.home()
  parser.add_argument(
      "--v8-dir", default=home / "v8", help="V8 repository directory")
  parser.add_argument(
      "--cb-dir", default=home / "crossbench", help="Crossbench directory")

  args = parser.parse_args()

  # 1. Upload CL
  # Assumes changes are already committed on the current branch.
  issue_out = run_command(["git", "cl", "issue"], cwd=args.v8_dir)
  if "None" in issue_out:
    upload_cmd = ["git", "cl", "upload", "-T", "-o", "wip"]
  else:
    upload_cmd = ["git", "cl", "upload", "-t", args.message]
  stdout = run_command(upload_cmd, cwd=args.v8_dir)
  print(stdout)

  # 2. Extract CL URL
  match = re.search(
      r"https://chromium-review\.googlesource\.com/c/v8/v8/\+/\d+", stdout)
  if not match:
    print("Failed to find CL URL in git cl upload output.")
    sys.exit(1)

  cl_url = match.group(0)
  print(f"Found CL URL: {cl_url}")

  # 3. Start Pinpoint job
  cb_cmd = [
      "./cb.py", "pinpoint", "start", f"--benchmark={args.benchmark}",
      f"--bot={args.bot}", f"--exp-patch={cl_url}"
  ]

  cb_stdout = run_command(cb_cmd, cwd=args.cb_dir)
  print(cb_stdout)


if __name__ == "__main__":
  main()
