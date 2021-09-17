#!/usr/bin/env python3
# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
from datetime import datetime
import re
import subprocess
import sys
from pathlib import Path

RE_GITHASH = re.compile(r"^[0-9a-f]{40}")
RE_AUTHOR_TIME = re.compile(r"^author-time (\d+)$")
RE_FILENAME = re.compile(r"^filename (.+)$")

VERSION_CACHE = dict()
RE_VERSION_MAJOR = re.compile(r".*V8_MAJOR_VERSION ([0-9]+)")
RE_VERSION_MINOR = re.compile(r".*V8_MINOR_VERSION ([0-9]+)")


def extract_version(hash):
  if hash in VERSION_CACHE:
    return VERSION_CACHE[hash]
  if hash == '0000000000000000000000000000000000000000':
    return 'HEAD'
  result = subprocess.check_output(
      ['git', 'show', f"{hash}:include/v8-version.h"], encoding='UTF-8')
  major = RE_VERSION_MAJOR.search(result).group(1)
  minor = RE_VERSION_MINOR.search(result).group(1)
  version = f"{major}.{minor}"
  VERSION_CACHE[hash] = version
  return version


def get_blame(file_path):
  result = subprocess.check_output(
      ['git', 'blame', '-t', '--line-porcelain', file_path], encoding='UTF-8')
  line_iter = iter(result.splitlines())
  blame_list = list()
  current_blame = None
  while True:
    line = next(line_iter, None)
    if line is None:
      break
    if RE_GITHASH.match(line):
      if current_blame is not None:
        blame_list.append(current_blame)
      hash = line.split(" ")[0]
      current_blame = {
          'datetime': 0,
          'filename': None,
          'content': None,
          'hash': hash
      }
      continue
    match = RE_AUTHOR_TIME.match(line)
    if match:
      current_blame['datetime'] = datetime.fromtimestamp(int(
          match.groups()[0]))
      continue
    match = RE_FILENAME.match(line)
    if match:
      current_blame['filename'] = match.groups()[0]
      current_blame['content'] = next(line_iter).strip()
      continue
  blame_list.append(current_blame)
  return blame_list


RE_MACRO_END = re.compile(r"\);")
RE_DEPRECATE_MACRO = re.compile(r"\(.*?,(.*)\);", re.MULTILINE)


def filter_and_print(blame_list, macro, options):
  before = options.before
  index = 0
  re_macro = re.compile(macro)
  deprecated = list()
  while index < len(blame_list):
    blame = blame_list[index]
    commit_datetime = blame['datetime']
    if commit_datetime >= before:
      index += 1
      continue
    line = blame['content']
    commit_hash = blame['hash']
    match = re_macro.search(line)
    if match:
      pos = match.end()
      start = -1
      parens = 0
      while True:
        if pos >= len(line):
          # Extend to next line
          index = index + 1
          blame = blame_list[index]
          line = line + blame['content']
        if line[pos] == '(':
          parens = parens + 1
        elif line[pos] == ')':
          parens = parens - 1
          if parens == 0:
            # Exclude closing ")
            pos = pos - 2
            break
        elif line[pos] == '"' and start == -1:
          start = pos + 1
        pos = pos + 1
      # Extract content and replace double quotes from merged lines
      content = line[start:pos].strip().replace('""', '')
      deprecated.append((index + 1, commit_datetime, commit_hash, content))
    index = index + 1
  print(f"# Marked as {macro}: {len(deprecated)}")
  for linenumber, commit_datetime, commit_hash, content in deprecated:
    commit_date = commit_datetime.date()
    file_position = (
        f"{options.v8_header}:{linenumber}").rjust(len(options.v8_header) + 5)
    print(f"   {file_position}\t{commit_date}\t{commit_hash[:8]}"
          f"\t{extract_version(commit_hash)}\t{content}")
  return len(deprecated)


def parse_options(args):
  parser = argparse.ArgumentParser(
      description="Collect deprecation statistics")
  parser.add_argument("v8_header", nargs='?', help="Path to v8.h")
  parser.add_argument("--before", help="Filter by date")
  options = parser.parse_args(args)
  if options.before:
    options.before = datetime.strptime(options.before, '%Y-%m-%d')
  else:
    options.before = datetime.now()
  if options.v8_header is None:
    base_path = Path(__file__).parent.parent
    options.v8_header = str(
        (base_path / 'include' / 'v8.h').relative_to(base_path))
  return options


def main(args):
  options = parse_options(args)
  blame_list = get_blame(options.v8_header)
  filter_and_print(blame_list, "V8_DEPRECATE_SOON", options)
  print("\n")
  filter_and_print(blame_list, "V8_DEPRECATED", options)


if __name__ == "__main__":
  main(sys.argv[1:])
