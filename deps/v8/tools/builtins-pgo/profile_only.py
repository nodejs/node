#!/usr/bin/env python3

# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

from pathlib import Path
import argparse
import subprocess
import sys


def main():
  args = parse_arguments()
  run_benchmark(args.benchmark_path, args.d8_path, args.output_dir)
  run_get_hints(args.output_dir, args.v8_target_cpu)


def parse_arguments():
  parser = argparse.ArgumentParser(
      description=('Generate builtin PGO profiles. '
                   'The script is designed to run in swarming context where '
                   'the isolate aready contains the instrumented binary.'))
  parser.add_argument(
      '--v8-target-cpu',
      default='pgo',
      help='target cpu to build the profile for: x64 or arm64')
  parser.add_argument(
      '--benchmark_path',
      default=Path('./JetStream2/cli.js'),
      help='path to benchmark runner .js file, usually JetStream2\'s `cli.js`',
      type=Path)
  parser.add_argument(
      '--d8-path',
      default=Path('./out/build/d8'),
      help=('path to the d8 executable, by default `./out/build/d8` in '
            'swarming context'),
      type=Path)
  parser.add_argument('--output-dir', type=Path)
  return parser.parse_args()


def run_benchmark(benchmark_path, d8_path, output_dir):
  root_dir = tools_pgo_dir().parent.parent
  benchmark_dir = (root_dir / benchmark_path).parent.absolute()
  assert benchmark_dir.exists(), "Could not find benchmark path!"

  benchmark_file = benchmark_path.name
  d8_path_abs = (root_dir / d8_path).absolute()
  assert d8_path_abs.exists(), "Could not find d8 path!"

  log_path = benchmark_log_path(output_dir)
  cmd = [d8_path_abs, f"--turbo-profiling-output={log_path}", benchmark_file]
  run(cmd, cwd=benchmark_dir)
  assert log_path.exists(), "Could not find benchmark logs path!"


def tools_pgo_dir():
  return Path(__file__).parent


def benchmark_log_path(output_dir):
  return (output_dir / "v8.builtins.pgo").absolute()


def run_get_hints(output_dir, v8_target_cpu):
  get_hints_path = (tools_pgo_dir() / "get_hints.py").absolute()
  assert get_hints_path.exists(), "Could not find get_hints.py script path!"

  profile_path = (output_dir / f"{v8_target_cpu}.profile").absolute()
  run([
      sys.executable, '-u', get_hints_path,
      benchmark_log_path(output_dir), profile_path
  ])
  assert profile_path.exists(), "Could not find profile path!"


def run(cmd, **kwargs):
  print(f"# CMD: {cmd} {kwargs}")
  subprocess.run(cmd, **kwargs, check=True)


if __name__ == "__main__":  # pragma: no cover
  sys.exit(main())
