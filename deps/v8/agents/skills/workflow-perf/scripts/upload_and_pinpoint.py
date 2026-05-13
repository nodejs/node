#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from __future__ import annotations

import argparse
import subprocess
import re
import sys
import shlex
from pathlib import Path


def run_command(cmd: list[str], cwd: str | None = None) -> str:
  print(f"Running: {shlex.join(cmd)}")
  result = subprocess.run(
      cmd, cwd=cwd, capture_output=True, text=True, check=False)
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
  upload_cmd = ["git", "cl", "upload", "-m", args.message]
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
