#!/usr/bin/env python3
# Copyright 2022 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys
import time

kDefaultSamplerateSecs = 0.001


def PrintHelp(returncode):
  print(f"""Usage: {sys.argv[0]} [--interval I] [--freq F] [--] COMMAND

Runs COMMAND, and samples its RSS memory usage every I milliseconds (or F \
times per second).
Default sample interval is {kDefaultSamplerateSecs * 1000} ms.
'--' as argument forces all further arguments to be part of COMMAND.
""")
  sys.exit(returncode)


def Main():
  # Parse arguments.
  args = []
  samplerate_secs = kDefaultSamplerateSecs
  idx = 1
  while idx < len(sys.argv):
    arg = sys.argv[idx]
    if arg == "--":
      args += sys.argv[(idx + 1):]
      break
    if arg in ("-h", "--help", "help"):
      PrintHelp(0)
    elif arg in ("--interval", "-I"):
      if idx + 1 >= len(sys.argv):
        PrintHelp(1)
      samplerate_secs = int(sys.argv[idx + 1]) / 1000
      idx += 1  # Skip the value.
    elif arg in ("--freq", "-F"):
      if idx + 1 >= len(sys.argv):
        PrintHelp(1)
      samplerate_secs = 1 / float(sys.argv[idx + 1])
      idx += 1  # Skip the value.
    else:
      # No match for known parameter; assume it's part of the command to run.
      args.append(arg)
    idx += 1  # Go to next arg.

  cmd = " ".join(args)
  print(f"sample rate: {samplerate_secs}")
  print(f"command: {cmd}")

  # Run the child process and observe it.
  process = subprocess.Popen(cmd, shell=True)
  pid = process.pid
  print(f"pid: {pid}")
  statusfile = f"/proc/{pid}/status"
  vmsum = 0
  observed_max = 0
  reported_max = 0
  sample_count = 0
  while process.poll() is None:
    with open(statusfile, 'r') as f:
      for line in f.read().splitlines():
        if line.startswith("VmRSS"):
          rss = int(line.split()[1])
          vmsum += rss
          if rss > observed_max:
            observed_max = rss
        elif line.startswith("VmHWM"):
          peak = int(line.split()[1])
          if peak > reported_max:
            reported_max = peak
      sample_count += 1
    time.sleep(samplerate_secs)

  # Report findings.
  print("\n")
  # TODO(jkummerow): See if this is accurate enough in practice.
  walltime = sample_count * samplerate_secs
  print(f"wall time:    {walltime:.3f} s")  # Sample rate precision!
  print("")
  avg = vmsum / sample_count / 1024
  reported_max /= 1024
  observed_max /= 1024
  print(f"average RSS:  {avg:.2f} MB")
  print(f"reported max: {reported_max:.2f} MB")
  print(f"observed max: {observed_max:.2f} MB")
  sys.exit(process.returncode)


if __name__ == "__main__":
  Main()
