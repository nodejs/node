#!/usr/bin/env python3

# Copyright 2023 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can
# be found in the LICENSE file.

from pathlib import Path
import argparse
import contextlib
import subprocess
import sys
import threading
import time


def main():
  args = parse_arguments()
  run_benchmark(args.benchmark_path, args.d8_path, args.output_dir)
  run_get_hints(args.output_dir, args.profile_name)


def parse_arguments():
  parser = argparse.ArgumentParser(
      description=('Generate builtin PGO profiles. '
                   'The script is designed to run in swarming context where '
                   'the isolate aready contains the instrumented binary.'))
  parser.add_argument(
      '--profile-name',
      default='pgo',
      help='target cpu to build the profile for: x64 or arm64')
  parser.add_argument(
      '--benchmark_path',
      # Note, this path entry is currently defining the JetStream version to
      # use, see: recipes/recipe_modules/v8_builtins_pgo/builders.py in the
      # build repository.
      default=Path('./JetStream3/cli.js'),
      help='path to benchmark runner .js file, usually JetStream3\'s `cli.js`',
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
  cmd = [d8_path_abs, f"--turbo-profiling-output={log_path}"]
  monitor_proc = False
  if "JetStream3" in str(benchmark_path):
    # TODO: remove once PGO builder works again.
    print_proc_stats()
    print_memory_stats()
    print_virtual_memory_stats()
    cmd.append("--trace-gc")
    monitor_proc = True
  cmd.append(benchmark_file)
  try:
    run(cmd, cwd=benchmark_dir, monitor=monitor_proc)
  except:
    if "JetStream3" in str(benchmark_path):
      # TODO: remove once PGO builder works again.
      print_proc_stats()
      print_memory_stats()
      print_virtual_memory_stats()
    raise
  assert log_path.exists(), "Could not find benchmark logs path!"


def print_proc_stats():
  print("#" * 80)
  print("# PROCESS STATS")
  print("#" * 80)
  sys.stdout.flush()
  with contextlib.suppress(Exception):
    if sys.platform.startswith("linux") or sys.platform == "darwin":
      subprocess.run(["ps", "-aux"])
    elif sys.platform == "win32":
      subprocess.run(["tasklist"])
    else:
      print(f"#   Process stats not implemented for platform: {sys.platform}")
  sys.stdout.flush()


def print_memory_stats():
  print("#" * 80)
  print("# Memory Stats:")
  print("#" * 80)
  sys.stdout.flush()
  with contextlib.suppress(Exception):
    if sys.platform.startswith("linux"):
      with open("/proc/meminfo", "r") as f:
        print(f.read(), end="")
    elif sys.platform == "darwin":
      subprocess.run(["vm_stat"])
    elif sys.platform == "win32":
      with contextlib.suppress(Exception):
        res = subprocess.run([
            "wmic",
            "OS",
            "get",
            "FreePhysicalMemory,TotalVisibleMemorySize,FreeVirtualMemory,TotalVirtualMemorySize",
            "/Value",
        ])
        if res.returncode == 0:
          return
      subprocess.run(["systeminfo"])
    else:
      print(f"#   Memory stats not implemented for platform: {sys.platform}")
  sys.stdout.flush()


def print_virtual_memory_stats():
  print("#" * 80)
  print("# VIRTUAL MEMORY STATS & LIMITS")
  print("#" * 80)
  sys.stdout.flush()
  with contextlib.suppress(Exception):
    if sys.platform.startswith("linux"):
      print("--- /proc/sys/vm/ ---")
      for vm_param in [
          "max_map_count",
          "overcommit_memory",
          "overcommit_ratio",
      ]:
        param_path = Path(f"/proc/sys/vm/{vm_param}")
        if param_path.exists():
          print(f"{vm_param}: {param_path.read_text().strip()}")
      print("\n--- /proc/self/limits ---")
      limits_path = Path("/proc/self/limits")
      if limits_path.exists():
        print(limits_path.read_text(), end="")
      print("\n--- /proc/self/status (Virtual Memory) ---")
      status_path = Path("/proc/self/status")
      if status_path.exists():
        for line in status_path.read_text().splitlines():
          if (line.startswith("Vm") or line.startswith("Rss") or
              "Threads" in line):
            print(line)
      maps_path = Path("/proc/self/maps")
      if maps_path.exists():
        print(f"\nCurrent process map count:"
              f" {len(maps_path.read_text().splitlines())}")
    elif sys.platform == "darwin":
      subprocess.run(["ulimit", "-a"])
      subprocess.run(["sysctl", "vm"])
    elif sys.platform == "win32":
      subprocess.run([
          "wmic",
          "pagefile",
          "get",
          "AllocatedBaseSize,CurrentUsage,PeakUsage",
          "/Value",
      ])
  sys.stdout.flush()


def tools_pgo_dir():
  return Path(__file__).parent


def benchmark_log_path(output_dir):
  return (output_dir / "v8.builtins.pgo").absolute()


def run_get_hints(output_dir, profile_name):
  get_hints_path = (tools_pgo_dir() / "get_hints.py").absolute()
  assert get_hints_path.exists(), "Could not find get_hints.py script path!"

  profile_path = (output_dir / f"{profile_name}.profile").absolute()
  run([
      sys.executable, '-u', get_hints_path,
      benchmark_log_path(output_dir), profile_path
  ])
  assert profile_path.exists(), "Could not find profile path!"


def _monitor_process(pid, stop_event, interval=5.0):
  while not stop_event.is_set():
    with contextlib.suppress(Exception):
      if not Path(f"/proc/{pid}").exists():
        break
      status_path = Path(f"/proc/{pid}/status")
      maps_path = Path(f"/proc/{pid}/maps")
      if status_path.exists():
        vmsize = vmpeak = vmrss = vmdat = vmstk = "N/A"
        for line in status_path.read_text().splitlines():
          if line.startswith("VmSize:"):
            vmsize = line.split(":", 1)[1].strip()
          elif line.startswith("VmPeak:"):
            vmpeak = line.split(":", 1)[1].strip()
          elif line.startswith("VmRSS:"):
            vmrss = line.split(":", 1)[1].strip()
          elif line.startswith("VmData:"):
            vmdat = line.split(":", 1)[1].strip()
          elif line.startswith("VmStk:"):
            vmstk = line.split(":", 1)[1].strip()
        map_count = (
            len(maps_path.read_text().splitlines())
            if maps_path.exists() else "N/A")
        print(f"# [MONITOR PID {pid}] VmSize: {vmsize} | VmPeak: {vmpeak} |"
              f" VmRSS: {vmrss} | VmData: {vmdat} | VmStk: {vmstk} | MapCount:"
              f" {map_count}")
        sys.stdout.flush()
    stop_event.wait(timeout=interval)


def run(cmd, monitor=False, **kwargs):
  print(f"# CMD: {cmd} {kwargs}")
  # TODO: remove once pgo investigation is complete.
  if monitor and sys.platform.startswith("linux"):
    proc = subprocess.Popen(cmd, **kwargs)
    stop_event = threading.Event()
    monitor_thread = threading.Thread(
        target=_monitor_process,
        args=(proc.pid, stop_event, 5.0),
        daemon=True,
    )
    monitor_thread.start()
    try:
      retcode = proc.wait()
    finally:
      stop_event.set()
      monitor_thread.join(timeout=2.0)
    if retcode != 0:
      raise subprocess.CalledProcessError(retcode, cmd)
  else:
    subprocess.run(cmd, **kwargs, check=True)


if __name__ == "__main__":  # pragma: no cover
  sys.exit(main())
