#!/usr/bin/env python3

# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

import subprocess
import argparse
from pathlib import Path

parser = argparse.ArgumentParser(
    description='Generate builtin PGO profiles. ' +
    'The script has to be run from the root of a V8 checkout and updates the profiles in `tools/builtins-pgo`.'
)
parser.add_argument(
    'v8_target_cpu', help='target cpu to build the profile for: x64 or arm64')
parser.add_argument(
    '--target-cpu',
    default=None,
    help='target cpu for V8 binary (for simulator builds), by default it\'s equal to `v8_target_cpu`'
)
parser.add_argument(
    'benchmark_path',
    help='path to benchmark runner .js file, usually JetStream2\'s `cli.js`',
    type=Path)
parser.add_argument(
    '--out-path',
    default=Path("out"),
    help='directory to be used for building V8, by default `./out`',
    type=Path)

args = parser.parse_args()

if args.target_cpu == None:
  args.target_cpu = args.v8_target_cpu


def run(cmd, **kwargs):
  print(f"# CMD: {cmd} {kwargs}")
  return subprocess.run(cmd, **kwargs)


def try_start_goma():
  res = run(["goma_ctl", "ensure_start"])
  print(res.returncode)
  has_goma = res.returncode == 0
  print("Detected Goma:", has_goma)
  return has_goma



def build_d8(path, gn_args):
  if not path.exists():
    path.mkdir(parents=True, exist_ok=True)
  with (path / "args.gn").open("w") as f:
    f.write(gn_args)
  run(["gn", "gen", path])
  run(["autoninja", "-C", path, "d8"])
  return (path / "d8").absolute()


tools_pgo_dir = Path(__file__).parent
v8_path = tools_pgo_dir.parent.parent

if not args.benchmark_path.is_file() or args.benchmark_path.suffix != ".js":
  print(f"Invalid benchmark argument: {args.benchmark_path}")
  exit(1)

has_goma_str = "true" if try_start_goma() else "false"

GN_ARGS_TEMPLATE = f"""\
is_debug = false
target_cpu = "{args.target_cpu}"
v8_target_cpu = "{args.v8_target_cpu}"
use_goma = {has_goma_str}
v8_enable_builtins_profiling = true
"""

for arch, gn_args in [(args.v8_target_cpu, GN_ARGS_TEMPLATE)]:
  build_dir = args.out_path / f"{arch}.release.generate_builtin_pgo_profile"
  d8_path = build_d8(build_dir, gn_args)
  benchmark_dir = args.benchmark_path.parent
  benchmark_file = args.benchmark_path.name
  log_path = (build_dir / "v8.builtins.pgo").absolute()
  run([d8_path, f"--turbo-profiling-output={log_path}", benchmark_file],
      cwd=benchmark_dir)
  get_hints_path = tools_pgo_dir / "get_hints.py"
  profile_path = tools_pgo_dir / f"{arch}.profile"
  run([get_hints_path, log_path, profile_path])
