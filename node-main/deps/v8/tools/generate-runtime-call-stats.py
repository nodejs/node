#!/usr/bin/python3
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Runs chromium/src/run_benchmark for a given story and extracts the generated
# runtime call stats.

import argparse
import csv
import json
import glob
import os
import pathlib
import re
import tabulate
import shutil
import statistics
import subprocess
import sys
import tempfile

from callstats_groups import RUNTIME_CALL_STATS_GROUPS


JSON_FILE_EXTENSION=".pb_converted.json"

def parse_args():
  parser = argparse.ArgumentParser(
      description="Run story and collect runtime call stats.")
  parser.add_argument("story", metavar="story", nargs=1, help="story to run")
  parser.add_argument(
      "--group",
      dest="group",
      action="store_true",
      help="group common stats together into buckets")
  parser.add_argument(
      "-r",
      "--repeats",
      dest="repeats",
      metavar="N",
      action="store",
      type=int,
      default=1,
      help="number of times to run the story")
  parser.add_argument(
      "-v",
      "--verbose",
      dest="verbose",
      action="store_true",
      help="output benchmark runs to stdout")
  parser.add_argument(
      "--device",
      dest="device",
      action="store",
      help="device to run the test on. Passed directly to run_benchmark")
  parser.add_argument(
      "-d",
      "--dir",
      dest="dir",
      action="store",
      help=("directory to look for already generated output in. This must "
            "already exists and it won't re-run the benchmark"))
  parser.add_argument(
      "-f",
      "--format",
      dest="format",
      action="store",
      choices=["csv", "table"],
      help="output as CSV")
  parser.add_argument(
      "-o",
      "--output",
      metavar="FILE",
      dest="out_file",
      action="store",
      help="write table to FILE rather stdout")
  parser.add_argument(
      "--browser",
      dest="browser",
      metavar="BROWSER_TYPE",
      action="store",
      default="release",
      help=("Passed directly to --browser option of run_benchmark. Ignored if "
            "-executable is used"))
  parser.add_argument(
      "-e",
      "--executable",
      dest="executable",
      metavar="EXECUTABLE",
      action="store",
      help=("path to executable to run. If not given it will pass '--browser "
            "release' to run_benchmark"))
  parser.add_argument(
      "--chromium-dir",
      dest="chromium_dir",
      metavar="DIR",
      action="store",
      default=".",
      help=("path to chromium directory. If not given, the script must be run "
            "inside the chromium/src directory"))
  parser.add_argument(
      "--js-flags", dest="js_flags", action="store", help="flags to pass to v8")
  parser.add_argument(
      "--extra-browser-args",
      dest="browser_args",
      action="store",
      help="flags to pass to chrome")
  parser.add_argument(
      "--benchmark",
      dest="benchmark",
      action="store",
      default="v8.browsing_desktop",
      help="benchmark to run")
  parser.add_argument(
      "--stdev",
      dest="stdev",
      action="store_true",
      help="adds columns for the standard deviation")
  parser.add_argument(
      "--filter",
      dest="filter",
      action="append",
      help="useable with --group to only show buckets specified by filter")
  parser.add_argument(
      "--retain",
      dest="retain",
      action="store",
      default="json",
      choices=["none", "json", "all"],
      help=("controls artifacts to be retained after run. With none, all files "
            "are deleted; only the json.gz file is retained for each run; and "
            "all keep all files"))

  return parser.parse_args()


def process_trace(trace_file):
  text_string = pathlib.Path(trace_file).read_text()
  result = json.loads(text_string)

  output = {}
  result = result["traceEvents"]
  for o in result:
    o = o["args"]
    if "runtime-call-stats" in o:
      r = o["runtime-call-stats"]
      for name in r:
        count = r[name][0]
        duration = r[name][1]
        if name in output:
          output[name]["count"] += count
          output[name]["duration"] += duration
        else:
          output[name] = {"count": count, "duration": duration}

  return output


def run_benchmark(story,
                  repeats=1,
                  output_dir=".",
                  verbose=False,
                  js_flags=None,
                  browser_args=None,
                  chromium_dir=".",
                  executable=None,
                  benchmark="v8.browsing_desktop",
                  device=None,
                  browser="release"):

  orig_chromium_dir = chromium_dir
  xvfb = os.path.join(chromium_dir, "testing", "xvfb.py")
  if not os.path.isfile(xvfb):
    chromium_dir = os.path(chromium_dir, "src")
    xvfb = os.path.join(chromium_dir, "testing", "xvfb.py")
    if not os.path.isfile(xvfb):
      print(("chromium_dir does not point to a valid chromium checkout: " +
             orig_chromium_dir))
      sys.exit(1)

  command = [
      xvfb,
      os.path.join(chromium_dir, "tools", "perf", "run_benchmark"),
      "run",
      "--story",
      story,
      "--pageset-repeat",
      str(repeats),
      "--output-dir",
      output_dir,
      "--intermediate-dir",
      os.path.join(output_dir, "artifacts"),
      benchmark,
  ]

  if executable:
    command += ["--browser-executable", executable]
  else:
    command += ["--browser", browser]

  if device:
    command += ["--device", device]
  if browser_args:
    command += ["--extra-browser-args", browser_args]
  if js_flags:
    command += ["--js-flags", js_flags]

  if not benchmark.startswith("v8."):
    # Most benchmarks by default don't collect runtime call stats so enable them
    # manually.
    categories = [
        "v8",
        "disabled-by-default-v8.runtime_stats",
    ]

    command += ["--extra-chrome-categories", ",".join(categories)]

  print("Output directory: %s" % output_dir)
  stdout = ""
  print(f"Running: {' '.join(command)}\n")
  proc = subprocess.Popen(
      command,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      universal_newlines=True)
  proc.stderr.close()
  status_matcher = re.compile(r"\[ +(\w+) +\]")
  for line in iter(proc.stdout.readline, ""):
    stdout += line
    match = status_matcher.match(line)
    if verbose or match:
      print(line, end="")

  proc.stdout.close()

  if proc.wait() != 0:
    print("\nrun_benchmark failed:")
    # If verbose then everything has already been printed.
    if not verbose:
      print(stdout)
    sys.exit(1)

  print("\nrun_benchmark completed")


def write_output(f, table, headers, run_count, format="table"):
  if format == "csv":
    # strip new lines from CSV output
    headers = [h.replace("\n", " ") for h in headers]
    writer = csv.writer(f)
    writer.writerow(headers)
    writer.writerows(table)
  else:
    # First column is name, and then they alternate between counts and durations
    summary_count = len(headers) - 2 * run_count - 1
    floatfmt = ("",) + (".0f", ".2f") * run_count + (".2f",) * summary_count
    f.write(tabulate.tabulate(table, headers=headers, floatfmt=floatfmt))
    f.write("\n")


class Row:

  def __init__(self, name, run_count):
    self.name = name
    self.durations = [0] * run_count
    self.counts = [0] * run_count
    self.mean_duration = None
    self.mean_count = None
    self.stdev_duration = None
    self.stdev_count = None

  def __repr__(self):
    data_str = ", ".join(
        str((c, d)) for (c, d) in zip(self.counts, self.durations))
    return (f"{self.name}: {data_str}, mean_count: {self.mean_count}, " +
            f"mean_duration: {self.mean_duration}")

  def add_data(self, counts, durations):
    self.counts = counts
    self.durations = durations

  def add_data_point(self, run, count, duration):
    self.counts[run] = count
    self.durations[run] = duration

  def prepare(self, stdev=False):
    if len(self.durations) > 1:
      self.mean_duration = statistics.mean(self.durations)
      self.mean_count = statistics.mean(self.counts)
      if stdev:
        self.stdev_duration = statistics.stdev(self.durations)
        self.stdev_count = statistics.stdev(self.counts)

  def as_list(self):
    l = [self.name]
    for (c, d) in zip(self.counts, self.durations):
      l += [c, d]
    if self.mean_duration is not None:
      l += [self.mean_count]
      if self.stdev_count is not None:
        l += [self.stdev_count]
      l += [self.mean_duration]
      if self.stdev_duration is not None:
        l += [self.stdev_duration]
    return l

  def key(self):
    if self.mean_duration is not None:
      return self.mean_duration
    else:
      return self.durations[0]


class Bucket:

  def __init__(self, name, run_count):
    self.name = name
    self.run_count = run_count
    self.data = {}
    self.table = None
    self.total_row = None

  def __repr__(self):
    s = "Bucket: " + self.name + " {\n"
    if self.table:
      s += "\n  ".join(str(row) for row in self.table) + "\n"
    elif self.data:
      s += "\n  ".join(str(row) for row in self.data.values()) + "\n"
    if self.total_row:
      s += "  " + str(self.total_row) + "\n"
    return s + "}"

  def add_data_point(self, name, run, count, duration):
    if name not in self.data:
      self.data[name] = Row(name, self.run_count)

    self.data[name].add_data_point(run, count, duration)

  def prepare(self, stdev=False):
    if self.data:
      for row in self.data.values():
        row.prepare(stdev)

      self.table = sorted(self.data.values(), key=Row.key)
      self.total_row = Row("Total", self.run_count)
      self.total_row.add_data([
          sum(r.counts[i]
              for r in self.data.values())
          for i in range(0, self.run_count)
      ], [
          sum(r.durations[i]
              for r in self.data.values())
          for i in range(0, self.run_count)
      ])
      self.total_row.prepare(stdev)

  def as_list(self, add_bucket_titles=True, filter=None):
    t = []
    if filter is None or self.name in filter:
      if add_bucket_titles:
        t += [["\n"], [self.name]]
      t += [r.as_list() for r in self.table]
      t += [self.total_row.as_list()]
    return t


def collect_buckets(story, group=True, repeats=1, output_dir="."):
  if group:
    groups = RUNTIME_CALL_STATS_GROUPS
  else:
    groups = []

  buckets = {}

  for i in range(0, repeats):
    story_dir = f"{story.replace(':', '_')}_{i + 1}"
    trace_dir = os.path.join(output_dir, "artifacts", story_dir, "trace",
                             "traceEvents")

    # run_benchmark now dumps two files: a .pb.gz file and a .pb_converted.json
    # file. We only need the latter.
    trace_file_glob = os.path.join(trace_dir, "*" + JSON_FILE_EXTENSION)
    trace_files = glob.glob(trace_file_glob)
    if not trace_files:
      print("Could not find *%s file in %s" % (JSON_FILE_EXTENSION, trace_dir))
      sys.exit(1)
    if len(trace_files) > 1:
      print("Expecting one file but got: %s" % trace_files)
      sys.exit(1)

    trace_file = trace_files[0]

    output = process_trace(trace_file)
    for name in output:
      bucket_name = "Other"
      for group in groups:
        if group[1].match(name):
          bucket_name = group[0]
          break

      value = output[name]
      if bucket_name not in buckets:
        bucket = Bucket(bucket_name, repeats)
        buckets[bucket_name] = bucket
      else:
        bucket = buckets[bucket_name]

      bucket.add_data_point(name, i, value["count"], value["duration"] / 1000.0)
  return buckets


def create_table(buckets, record_bucket_names=True, filter=None):
  table = []
  for bucket in buckets.values():
    table += bucket.as_list(
        add_bucket_titles=record_bucket_names, filter=filter)
  return table


def main():
  args = parse_args()
  story = args.story[0]

  retain = args.retain
  if args.dir is not None:
    output_dir = args.dir
    if not os.path.isdir(output_dir):
      print("Specified output directory does not exist: " % output_dir)
      sys.exit(1)
  else:
    output_dir = tempfile.mkdtemp(prefix="runtime_call_stats_")
    run_benchmark(
        story,
        repeats=args.repeats,
        output_dir=output_dir,
        verbose=args.verbose,
        js_flags=args.js_flags,
        browser_args=args.browser_args,
        chromium_dir=args.chromium_dir,
        benchmark=args.benchmark,
        executable=args.executable,
        browser=args.browser,
        device=args.device)

  try:
    buckets = collect_buckets(
        story, group=args.group, repeats=args.repeats, output_dir=output_dir)

    for b in buckets.values():
      b.prepare(args.stdev)

    table = create_table(
        buckets, record_bucket_names=args.group, filter=args.filter)

    headers = [""] + ["Count", "Duration\n(ms)"] * args.repeats
    if args.repeats > 1:
      if args.stdev:
        headers += [
            "Count\nMean", "Count\nStdev", "Duration\nMean (ms)",
            "Duration\nStdev (ms)"
        ]
      else:
        headers += ["Count\nMean", "Duration\nMean (ms)"]

    if args.out_file:
      with open(args.out_file, "w", newline="") as f:
        write_output(f, table, headers, args.repeats, args.format)
    else:
      write_output(sys.stdout, table, headers, args.repeats, args.format)
  finally:
    if retain == "none":
      shutil.rmtree(output_dir)
    elif retain == "json":
      # Delete all files bottom up except ones ending in JSON_FILE_EXTENSION and
      # attempt to delete subdirectories (ignoring errors).
      for dir_name, subdir_list, file_list in os.walk(
          output_dir, topdown=False):
        for file_name in file_list:
          if not file_name.endswith(JSON_FILE_EXTENSION):
            os.remove(os.path.join(dir_name, file_name))
        for subdir in subdir_list:
          try:
            os.rmdir(os.path.join(dir_name, subdir))
          except OSError:
            pass


if __name__ == "__main__":
  sys.exit(main())
