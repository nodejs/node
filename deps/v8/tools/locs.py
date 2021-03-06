#!/usr/bin/env python

# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

""" locs.py - Count lines of code before and after preprocessor expansion
  Consult --help for more information.
"""

# for py2/py3 compatibility
from __future__ import print_function

import argparse
import json
import multiprocessing
import os
import re
import subprocess
import sys
import tempfile
import time
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

# for py2/py3 compatibility
try:
  FileNotFoundError
except NameError:
  FileNotFoundError = IOError

ARGPARSE = argparse.ArgumentParser(
    description=("A script that computes LoC for a build dir"),
    epilog="""Examples:
 Count with default settings for build in out/Default:
   locs.py --build-dir out/Default
 Count only a custom group of files settings for build in out/Default:
   tools/locs.py --build-dir out/Default
                 --group src-compiler '\.\./\.\./src/compiler'
                 --only src-compiler
 Report the 10 files with the worst expansion:
   tools/locs.py --build-dir out/Default --worst 10
 Report the 10 files with the worst expansion in src/compiler:
   tools/locs.py --build-dir out/Default --worst 10
                 --group src-compiler '\.\./\.\./src/compiler'
                 --only src-compiler
 Report the 10 largest files after preprocessing:
   tools/locs.py --build-dir out/Default --largest 10
 Report the 10 smallest input files:
   tools/locs.py --build-dir out/Default --smallest 10""",
    formatter_class=argparse.RawTextHelpFormatter
)

ARGPARSE.add_argument(
    '--json',
    action='store_true',
    default=False,
    help="output json instead of short summary")
ARGPARSE.add_argument(
    '--build-dir',
    type=str,
    help="Use specified build dir and generate necessary files",
    required=True)
ARGPARSE.add_argument(
    '--echocmd',
    action='store_true',
    default=False,
    help="output command used to compute LoC")
ARGPARSE.add_argument(
    '--only',
    action='append',
    default=[],
    help="Restrict counting to report group (can be passed multiple times)")
ARGPARSE.add_argument(
    '--not',
    action='append',
    default=[],
    help="Exclude specific group (can be passed multiple times)")
ARGPARSE.add_argument(
    '--list-groups',
    action='store_true',
    default=False,
    help="List groups and associated regular expressions")
ARGPARSE.add_argument(
    '--group',
    nargs=2,
    action='append',
    default=[],
    help="Add a report group (can be passed multiple times)")
ARGPARSE.add_argument(
    '--largest',
    type=int,
    nargs='?',
    default=0,
    const=3,
    help="Output the n largest files after preprocessing")
ARGPARSE.add_argument(
    '--worst',
    type=int,
    nargs='?',
    default=0,
    const=3,
    help="Output the n files with worst expansion by preprocessing")
ARGPARSE.add_argument(
    '--smallest',
    type=int,
    nargs='?',
    default=0,
    const=3,
    help="Output the n smallest input files")
ARGPARSE.add_argument(
    '--files',
    type=int,
    nargs='?',
    default=0,
    const=3,
    help="Output results for each file separately")
ARGPARSE.add_argument(
    '--jobs',
    type=int,
    default=multiprocessing.cpu_count(),
    help="Process specified number of files concurrently")

ARGS = vars(ARGPARSE.parse_args())


def MaxWidth(strings):
  max_width = 0
  for s in strings:
    max_width = max(max_width, len(s))
  return max_width


def GenerateCompileCommandsAndBuild(build_dir, out):
  if not os.path.isdir(build_dir):
    print("Error: Specified build dir {} is not a directory.".format(
        build_dir), file=sys.stderr)
    exit(1)

  autoninja = "autoninja -C {}".format(build_dir)
  if subprocess.call(autoninja, shell=True, stdout=out) != 0:
    print("Error: Building {} failed.".format(build_dir), file=sys.stderr)
    exit(1)

  compile_commands_file = "{}/compile_commands.json".format(build_dir)
  print("Generating compile commands in {}.".format(
      compile_commands_file), file=out)
  ninja = "ninja -C {} -t compdb cxx cc > {}".format(
      build_dir, compile_commands_file)
  if subprocess.call(ninja, shell=True, stdout=out) != 0:
    print("Error: Cound not generate {} for {}.".format(
        compile_commands_file, build_dir), file=sys.stderr)
    exit(1)

  ninja_deps_file = "{}/ninja-deps.txt".format(build_dir)
  print("Generating ninja dependencies in {}.".format(
      ninja_deps_file), file=out)
  ninja = "ninja -C {} -t deps > {}".format(
      build_dir, ninja_deps_file)
  if subprocess.call(ninja, shell=True, stdout=out) != 0:
    print("Error: Cound not generate {} for {}.".format(
        ninja_deps_file, build_dir), file=sys.stderr)
    exit(1)

  return compile_commands_file, ninja_deps_file


def fmt_bytes(num_bytes):
  if num_bytes > 1024*1024*1024:
    return int(num_bytes / (1024*1024)), "MB"
  elif num_bytes > 1024*1024:
    return int(num_bytes / (1024)), "kB"
  return int(num_bytes), " B"


class CompilationData:
  def __init__(self, loc, in_bytes, expanded, expanded_bytes):
    self.loc = loc
    self.in_bytes = in_bytes
    self.expanded = expanded
    self.expanded_bytes = expanded_bytes

  def ratio(self):
    return self.expanded / (self.loc+1)

  def to_string(self):
    exp_bytes, exp_unit = fmt_bytes(self.expanded_bytes)
    in_bytes, in_unit = fmt_bytes(self.in_bytes)
    return "{:>9,} LoC ({:>7,} {}) to {:>12,} LoC ({:>7,} {}) ({:>5.0f}x)".format(
        self.loc, in_bytes, in_unit, self.expanded, exp_bytes, exp_unit, self.ratio())


class File(CompilationData):
  def __init__(self, file, target, loc, in_bytes, expanded, expanded_bytes):
    super().__init__(loc, in_bytes, expanded, expanded_bytes)
    self.file = file
    self.target = target

  def to_string(self):
    return "{} {} {}".format(super().to_string(), self.file, self.target)


class Group(CompilationData):
  def __init__(self, name, regexp_string):
    super().__init__(0, 0, 0, 0)
    self.name = name
    self.count = 0
    self.regexp = re.compile(regexp_string)

  def account(self, unit):
    if (self.regexp.match(unit.file)):
      self.loc += unit.loc
      self.in_bytes += unit.in_bytes
      self.expanded += unit.expanded
      self.expanded_bytes += unit.expanded_bytes
      self.count += 1

  def to_string(self, name_width):
    return "{:<{}} ({:>5} files): {}".format(
        self.name, name_width, self.count, super().to_string())


def SetupReportGroups():
  default_report_groups = {"total": '.*',
                           "src": '\\.\\./\\.\\./src',
                           "test": '\\.\\./\\.\\./test',
                           "third_party": '\\.\\./\\.\\./third_party',
                           "gen": 'gen'}

  report_groups = default_report_groups.copy()
  report_groups.update(dict(ARGS['group']))

  if ARGS['only']:
    for only_arg in ARGS['only']:
      if not only_arg in report_groups.keys():
        print("Error: specified report group '{}' is not defined.".format(
            ARGS['only']))
        exit(1)
      else:
        report_groups = {
            k: v for (k, v) in report_groups.items() if k in ARGS['only']}

  if ARGS['not']:
    report_groups = {
        k: v for (k, v) in report_groups.items() if k not in ARGS['not']}

  if ARGS['list_groups']:
    print_cat_max_width = MaxWidth(list(report_groups.keys()) + ["Category"])
    print("  {:<{}}  {}".format("Category",
                                print_cat_max_width, "Regular expression"))
    for cat, regexp_string in report_groups.items():
      print("  {:<{}}: {}".format(
          cat, print_cat_max_width, regexp_string))

  report_groups = {k: Group(k, v) for (k, v) in report_groups.items()}

  return report_groups


class Results:
  def __init__(self):
    self.groups = SetupReportGroups()
    self.units = {}
    self.source_dependencies = {}
    self.header_dependents = {}

  def track(self, filename):
    is_tracked = False
    for group in self.groups.values():
      if group.regexp.match(filename):
        is_tracked = True
    return is_tracked

  def recordFile(self, filename, targetname, loc, in_bytes, expanded, expanded_bytes):
    unit = File(filename, targetname, loc, in_bytes, expanded, expanded_bytes)
    self.units[filename] = unit
    for group in self.groups.values():
      group.account(unit)

  def maxGroupWidth(self):
    return MaxWidth([v.name for v in self.groups.values()])

  def printGroupResults(self, file):
    for key in sorted(self.groups.keys()):
      print(self.groups[key].to_string(self.maxGroupWidth()), file=file)

  def printSorted(self, key, count, reverse, out):
    for unit in sorted(list(self.units.values()), key=key, reverse=reverse)[:count]:
      print(unit.to_string(), file=out)

  def addHeaderDeps(self, source_dependencies, header_dependents):
    self.source_dependencies = source_dependencies
    self.header_dependents = header_dependents


class LocsEncoder(json.JSONEncoder):
  def default(self, o):
    if isinstance(o, File):
      return {"file": o.file, "target": o.target, "loc": o.loc, "in_bytes": o.in_bytes,
              "expanded": o.expanded, "expanded_bytes": o.expanded_bytes}
    if isinstance(o, Group):
      return {"name": o.name, "loc": o.loc, "in_bytes": o.in_bytes,
              "expanded": o.expanded, "expanded_bytes": o.expanded_bytes}
    if isinstance(o, Results):
      return {"groups": o.groups, "units": o.units,
              "source_dependencies": o.source_dependencies,
              "header_dependents": o.header_dependents}
    return json.JSONEncoder.default(self, o)


class StatusLine:
  def __init__(self):
    self.max_width = 0

  def print(self, statusline, end="\r", file=sys.stdout):
    self.max_width = max(self.max_width, len(statusline))
    print("{0:<{1}}".format(statusline, self.max_width),
          end=end, file=file, flush=True)


class CommandSplitter:
  def __init__(self):
    self.cmd_pattern = re.compile(
        "([^\\s]*\\s+)?(?P<clangcmd>[^\\s]*clang.*)"
        " -c (?P<infile>.*) -o (?P<outfile>.*)")

  def process(self, compilation_unit):
    cmd = self.cmd_pattern.match(compilation_unit['command'])
    outfilename = cmd.group('outfile')
    infilename = cmd.group('infile')
    infile = Path(compilation_unit['directory']).joinpath(infilename)
    return (cmd.group('clangcmd'), infilename, infile, outfilename)


def parse_ninja_deps(ninja_deps):
  source_dependencies = {}
  header_dependents = defaultdict(int)
  current_target = None
  for line in ninja_deps:
    line = line.rstrip()
    # Ignore empty lines
    if not line:
      current_target = None
      continue
    if line[0] == ' ':
      # New dependency
      if len(line) < 5 or line[0:4] != '    ' or line[5] == ' ':
        sys.exit('Lines must have no indentation or exactly four ' +
                 'spaces.')
      dep = line[4:]
      if not re.search(r"\.(h|hpp)$", dep):
        continue
      header_dependents[dep] += 1
      continue
    # New target
    colon_pos = line.find(':')
    if colon_pos < 0:
      sys.exit('Unindented line must have a colon')
    if current_target is not None:
      sys.exit('Missing empty line before new target')
    current_target = line[0:colon_pos]
    match = re.search(r"#deps (\d+)", line)
    deps_number = match.group(1)
    source_dependencies[current_target] = int(deps_number)

  return (source_dependencies, header_dependents)


def Main():
  out = sys.stdout
  if ARGS['json']:
    out = sys.stderr

  compile_commands_file, ninja_deps_file = GenerateCompileCommandsAndBuild(
      ARGS['build_dir'], out)

  result = Results()
  status = StatusLine()

  try:
    with open(compile_commands_file) as file:
      compile_commands = json.load(file)
    with open(ninja_deps_file) as file:
      source_dependencies, header_dependents = parse_ninja_deps(file)
      result.addHeaderDeps(source_dependencies, header_dependents)
  except FileNotFoundError:
    print("Error: Cannot read '{}'. Consult --help to get started.".format(
        ninja_deps_file))
    exit(1)

  cmd_splitter = CommandSplitter()

  def count_lines_of_unit(ikey):
    i, key = ikey
    if not result.track(key['file']):
      return
    message = "[{}/{}] Counting LoCs of {}".format(
        i, len(compile_commands), key['file'])
    status.print(message, file=out)
    clangcmd, infilename, infile, outfilename = cmd_splitter.process(key)
    if not infile.is_file():
      return

    clangcmd = clangcmd + " -E -P " + \
        str(infile) + " -o /dev/stdout | sed '/^\\s*$/d' | wc -lc"
    loccmd = ("cat {}  | sed '\\;^\\s*//;d' | sed '\\;^/\\*;d'"
              " | sed '/^\\*/d' | sed '/^\\s*$/d' | wc -lc")
    loccmd = loccmd.format(infile)
    runcmd = " {} ; {}".format(clangcmd, loccmd)
    if ARGS['echocmd']:
      print(runcmd)
    process = subprocess.Popen(
        runcmd, shell=True, cwd=key['directory'], stdout=subprocess.PIPE)
    p = {'process': process, 'infile': infilename, 'outfile': outfilename}
    output, _ = p['process'].communicate()
    expanded, expanded_bytes, loc, in_bytes = list(map(int, output.split()))
    result.recordFile(p['infile'], p['outfile'], loc,
                      in_bytes, expanded, expanded_bytes)

  with tempfile.TemporaryDirectory(dir='/tmp/', prefix="locs.") as temp:
    start = time.time()

    with ThreadPoolExecutor(max_workers=ARGS['jobs']) as executor:
      list(executor.map(count_lines_of_unit, enumerate(compile_commands)))

    end = time.time()
    if ARGS['json']:
      print(json.dumps(result, ensure_ascii=False, cls=LocsEncoder))
    status.print("Processed {:,} files in {:,.2f} sec.".format(
        len(compile_commands), end-start), end="\n", file=out)
    result.printGroupResults(file=out)

    if ARGS['largest']:
      print("Largest {} files after expansion:".format(ARGS['largest']))
      result.printSorted(
          lambda v: v.expanded, ARGS['largest'], reverse=True, out=out)

    if ARGS['worst']:
      print("Worst expansion ({} files):".format(ARGS['worst']))
      result.printSorted(
          lambda v: v.ratio(), ARGS['worst'], reverse=True, out=out)

    if ARGS['smallest']:
      print("Smallest {} input files:".format(ARGS['smallest']))
      result.printSorted(
          lambda v: v.loc, ARGS['smallest'], reverse=False, out=out)

    if ARGS['files']:
      print("List of input files:")
      result.printSorted(
          lambda v: v.file, ARGS['files'], reverse=False, out=out)

  return 0


if __name__ == '__main__':
  sys.exit(Main())
