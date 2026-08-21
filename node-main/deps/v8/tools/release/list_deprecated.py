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
import logging
from multiprocessing import Pool

RE_GITHASH = re.compile(r"^[0-9a-f]{40}")
RE_AUTHOR_TIME = re.compile(r"^author-time (\d+)$")
RE_FILENAME = re.compile(r"^filename (.+)$")

VERSION_CACHE = dict()
RE_VERSION_MAJOR = re.compile(r".*V8_MAJOR_VERSION ([0-9]+)")
RE_VERSION_MINOR = re.compile(r".*V8_MINOR_VERSION ([0-9]+)")

RE_MACRO_END = re.compile(r"\);")
RE_DEPRECATE_MACRO = re.compile(r"\(.*?,(.*)\);", re.MULTILINE)


class HeaderFile(object):
  def __init__(self, path):
    self.path = path
    self.blame_list = self.get_blame_list()

  @classmethod
  def get_api_header_files(cls, options):
    files = subprocess.check_output(
        ['git', 'ls-tree', '--name-only', '-r', 'HEAD', options.include_dir],
        encoding='UTF-8')
    files = map(Path, filter(lambda l: l.endswith('.h'), files.splitlines()))
    with Pool(processes=24) as pool:
      return pool.map(cls, files)

  def extract_version(self, hash):
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

  def get_blame_list(self):
    logging.info(f"blame list for {self.path}")
    result = subprocess.check_output(
        ['git', 'blame', '-t', '--line-porcelain', self.path],
        encoding='UTF-8')
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
        current_blame['datetime'] = datetime.fromtimestamp(
            int(match.groups()[0]))
        continue
      match = RE_FILENAME.match(line)
      if match:
        current_blame['filename'] = match.groups()[0]
        current_blame['content'] = next(line_iter).strip()
        continue
    blame_list.append(current_blame)
    return blame_list

  def filter_and_print(self, macro, options):
    before = options.before
    index = 0
    re_macro = re.compile(macro)
    deprecated = list()
    while index < len(self.blame_list):
      blame = self.blame_list[index]
      line = blame['content']
      if line.startswith("#") or line.startswith("//"):
        index += 1
        continue
      commit_datetime = blame['datetime']
      if commit_datetime >= before:
        index += 1
        continue
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
            blame = self.blame_list[index]
            line = line + blame['content']
          if line[pos] == '(':
            parens = parens + 1
          elif line[pos] == ')':
            parens = parens - 1
            if parens == 0:
              # Exclude closing ")
              pos = pos - 1
              break
          elif line[pos] == '"' and start == -1:
            start = pos + 1
          pos = pos + 1
        # Extract content and replace double quotes from merged lines
        content = line[start:pos].strip().replace('""', '')
        deprecated.append((index + 1, commit_datetime, commit_hash, content))
      index = index + 1
    for linenumber, commit_datetime, commit_hash, content in deprecated:
      self.print_details(linenumber, commit_datetime, commit_hash, content)

  def print_details(self, linenumber, commit_datetime, commit_hash, content):
    commit_date = commit_datetime.date()
    file_position = (f"{self.path}:{linenumber}").ljust(40)
    v8_version = f"v{self.extract_version(commit_hash)}".rjust(5)
    print(f"{file_position}  {v8_version}  {commit_date}  {commit_hash[:8]}"
          f"  {content}")

  def print_v8_version(self, options):
    commit_hash, commit_datetime = subprocess.check_output(
        ['git', 'log', '-1', '--format=%H%n%ct', self.path],
        encoding='UTF-8').splitlines()
    commit_datetime = datetime.fromtimestamp(int(commit_datetime))
    self.print_details(11, commit_datetime, commit_hash, content="")


def parse_options(args):
  parser = argparse.ArgumentParser(
      description="Collect deprecation statistics")
  parser.add_argument("include_dir", nargs='?', help="Path to includes dir")
  parser.add_argument("--before", help="Filter by date")
  parser.add_argument("--verbose",
                      "-v",
                      help="Verbose logging",
                      action="store_true")
  options = parser.parse_args(args)
  if options.verbose:
    logging.basicConfig(level=logging.DEBUG)
  if options.before:
    options.before = datetime.strptime(options.before, '%Y-%m-%d')
  else:
    options.before = datetime.now()
  if options.include_dir is None:
    base_path = Path(__file__).parent.parent
    options.include_dir = str((base_path / 'include').relative_to(base_path))
  return options


def main(args):
  options = parse_options(args)

  print("# CURRENT V8 VERSION:")
  version = HeaderFile(Path(options.include_dir) / 'v8-version.h')
  version.print_v8_version(options)

  header_files = HeaderFile.get_api_header_files(options)
  print("\n")
  print("# V8_DEPRECATE_SOON:")
  for header in header_files:
    header.filter_and_print("V8_DEPRECATE_SOON", options)

  print("\n")
  print("# V8_DEPRECATED:")
  for header in header_files:
    header.filter_and_print("V8_DEPRECATED", options)


if __name__ == "__main__":
  main(sys.argv[1:])
