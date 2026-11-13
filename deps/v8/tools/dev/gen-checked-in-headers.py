#!/usr/bin/env python3

# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.
"""Regenerate headers in src/gen/ with `gm.py gen-checked-in-headers`.

Builds regen_checked_in_generated_headers in out/<arch>.release, creating
that build directory if needed. Requires v8_use_checked_in_generated_headers.
"""

import argparse
import os
import platform
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser(
    description='Regenerates the checked-in generated headers in src/gen/.')
parser.add_argument(
    '--out',
    default=Path('out'),
    required=False,
    type=Path,
    help='directory containing the build directories')
args = parser.parse_args()

GN_ARG = "v8_use_checked_in_generated_headers"
TARGET = "regen_checked_in_generated_headers"


def run(cmd, **kwargs):
  print(f"# CMD: {cmd}")
  return subprocess.run(cmd, **kwargs, check=True)


def machine_target():
  raw = platform.machine().lower()
  if raw in ("x86_64", "amd64"):
    return "x64"
  if raw == "aarch64":
    return "arm64"
  return raw


V8_PATH = Path(__file__).parents[2]
GM = V8_PATH / 'tools' / 'dev' / 'gm.py'


def main():
  target = machine_target()
  build_dir = args.out / f"{target}.release"

  if not (V8_PATH / build_dir / "args.gn").exists():
    env = os.environ.copy()
    env["V8_GM_OUTDIR"] = str((V8_PATH / args.out).resolve())
    run([sys.executable, str(GM), f"{target}.release.gn_args"],
        cwd=V8_PATH,
        env=env)
    run(["gn", "gen", str(build_dir)], cwd=V8_PATH)

  gn_args = subprocess.run(
      ["gn", "args",
       str(build_dir), f"--list={GN_ARG}", "--short"],
      cwd=V8_PATH,
      capture_output=True,
      text=True,
      check=True).stdout
  if f"{GN_ARG} = true" not in gn_args:
    print(
        f"{build_dir} has {GN_ARG} = false: its generated headers are not "
        "the ones that are checked in, so regenerating from it would check "
        "in the wrong content. Use a build directory with the default "
        "configuration. See src/gen/README.md.",
        file=sys.stderr)
    return 1

  # The action writes into src/gen/, which is outside the build directory
  # and so cannot be declared as an output; its only output is a stamp
  # file, and a hand-edited or deleted checked-in copy does not invalidate
  # it. Remove it so the action always runs.
  (V8_PATH / build_dir / "gen" / f"{TARGET}.stamp").unlink(missing_ok=True)

  run(["autoninja", "-C", str(build_dir), TARGET], cwd=V8_PATH)
  return 0


if __name__ == "__main__":
  sys.exit(main())
