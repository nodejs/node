#!/usr/bin/env python3
# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Scores the runs of a fair comparison.

For each run directory, prints the first find of the bug, the number of
matching crashes, the coverage at the given hours, the executions and the run
time. A find is a crash that matches the signature and that reproduces. For
Redline, a rerun must crash with the same signature: "same_signature" of 1 or
more in the crash event ("reproducible" for builds without that field). For
Fuzzilli, the name of the crash file must contain "deterministic".

A run directory is a Redline output directory (it has events.jsonl) or a
Fuzzilli storage directory (it has settings.json). run_arm.sh writes the start
time of each run to RUN.start, next to the directory. Fuzzilli runs need it.

For Fuzzilli, the signature must match the d8 output in the STDERR part of the
crash file, for example "foo.cc, line 123". For Redline, it must match the
crash signature, for example "fatal foo.cc:123". If the Redline signature has
only a signal or an exit code, the script also searches the stderr of the
crash event. It ignores signal 15, which comes from stopped d8 processes.

A coverage cell shows "-" when the run has no sample near that hour, for
example because the run stopped early. Copy run directories with "rsync -a",
"cp -a" or tar, because the script reads the file times of Fuzzilli.

Without a signature, the script scores only the coverage, for example of the
coverage setup.

Usage: score_runs.py [--signature=REGEX | --signature-file=FILE]
                     [--hours=1,4,8] RUN...
"""

import argparse
import datetime
import glob
import json
import os
import re
import sys


def read_start(run):
  """Returns the start time of a run in seconds since the epoch, or None."""
  path = run.rstrip("/") + ".start"
  if not os.path.exists(path):
    return None
  stamp = open(path).readline().strip()
  return datetime.datetime.fromisoformat(stamp).timestamp()


# A Redline signature without frames or a check message.
SIGNAL_ONLY = re.compile(r"^(sig|exit)\d+( [A-Z_]+)?$")


def redline_run(run, pattern):
  """Scores a Redline output directory."""
  find, matches, reproducible, series = None, 0, 0, []
  for line in open(os.path.join(run, "events.jsonl"), errors="replace"):
    if '"crash"' not in line and '"stats"' not in line:
      continue
    try:
      event = json.loads(line)
    except ValueError:
      continue
    if event.get("event") == "stats":
      coverage = event.get("coverage", 0.0)
      series.append((event["t"], coverage, event.get("executions", 0)))
    elif event.get("event") == "crash":
      signature = event.get("signature", "")
      if signature.startswith("sig15"):
        continue
      text = signature
      if SIGNAL_ONLY.match(signature):
        text += "\n" + event.get("stderr", "")
      if pattern.search(text):
        matches += 1
        if "same_signature" in event:
          repro = event["same_signature"] >= 1
        else:
          repro = bool(event.get("reproducible"))
        if repro:
          reproducible += 1
          if find is None:
            find = event["t"]
  return find, matches, reproducible, series


def stderr_part(text):
  """Returns the STDERR part of the crash information of a Fuzzilli crash
  file, or the whole text if it has no such part."""
  begin = text.find("STDERR:")
  if begin < 0:
    return text
  end = text.find("STDOUT:", begin)
  return text[begin:end if end >= 0 else len(text)]


def fuzzilli_run(run, pattern):
  """Scores a Fuzzilli storage directory."""
  start = read_start(run)
  if start is None:
    print("%s: no .start file, so no times" % run, file=sys.stderr)
  files = glob.glob(os.path.join(run, "crashes", "*.js"))
  files += glob.glob(os.path.join(run, "crashes", "duplicates", "*.js"))
  find, matches, reproducible = None, 0, 0
  for path in sorted(files, key=os.path.getmtime):
    if not pattern.search(stderr_part(open(path, errors="replace").read())):
      continue
    matches += 1
    if path.endswith("_deterministic.js"):
      reproducible += 1
      if find is None and start is not None:
        find = os.path.getmtime(path) - start
  series = []
  for path in sorted(glob.glob(os.path.join(run, "stats", "*.json"))):
    try:
      stats = json.load(open(path))
    except ValueError:
      continue
    if start is not None:
      coverage = float(stats.get("coverage", 0.0))
      executions = int(float(stats.get("totalExecs", 0)))
      series.append((os.path.getmtime(path) - start, coverage, executions))
  return find, matches, reproducible, series


def at(series, seconds, interval):
  """Returns the last sample at or before `seconds` (plus 60 s), or None if
  that sample is more than one stats interval before `seconds`."""
  best = None
  for sample in series:
    if sample[0] <= seconds + 60:
      best = sample
  if best is None or best[0] < seconds - interval - 60:
    return None
  return best


def main():
  parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
  parser.add_argument("--signature", help="a regular expression")
  parser.add_argument("--signature-file", help="a file with the expression")
  parser.add_argument("--hours", default="1,4,8")
  parser.add_argument("runs", nargs="+")
  args = parser.parse_args()
  if args.signature and args.signature_file:
    parser.error("give --signature or --signature-file, not both")
  if args.signature_file:
    regex = open(args.signature_file).read().strip()
  else:
    regex = args.signature or "(?!)"  # Matches nothing: coverage only.
  pattern = re.compile(regex)
  hours = [float(h) for h in args.hours.split(",") if h]

  for run in args.runs:
    if os.path.exists(os.path.join(run, "events.jsonl")):
      tool, result = "redline", redline_run(run, pattern)
      interval = 60
    elif os.path.exists(os.path.join(run, "settings.json")):
      tool, result = "fuzzilli", fuzzilli_run(run, pattern)
      # Fuzzilli writes stats every 10 minutes by default. run_arm.sh sets 1.
      interval = 600
    else:
      print("%s: not a run directory" % run, file=sys.stderr)
      continue
    find, matches, reproducible, series = result
    cells = []
    for h in hours:
      sample = at(series, h * 3600, interval)
      cell = "%.2f%%" % (100 * sample[1]) if sample else "-"
      cells.append("%gh %s" % (h, cell))
    last = series[-1] if series else (0, 0.0, 0)
    name = os.path.basename(run.rstrip("/"))
    found = "%.0fs" % find if find is not None else "no"
    end = "end %.2f h, %.2f%%, %d execs" % (last[0] / 3600, 100 * last[1],
                                            last[2])
    print("%-40s %-8s find %-9s matches %d (%d reproducible)  %s | %s" %
          (name, tool, found, matches, reproducible, "  ".join(cells), end))


if __name__ == "__main__":
  main()
