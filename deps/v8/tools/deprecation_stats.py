#!/usr/bin/env python
# Copyright 2018 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
from datetime import datetime
import re
import subprocess
import sys

RE_GITHASH = re.compile(r"^[0-9a-f]{40}")
RE_AUTHOR_TIME = re.compile(r"^author-time (\d+)$")
RE_FILENAME = re.compile(r"^filename (.+)$")

def GetBlame(file_path):
  result = subprocess.check_output(
      ['git', 'blame', '-t', '--line-porcelain', file_path])
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
      current_blame = {'time': 0, 'filename': None, 'content': None}
      continue
    match = RE_AUTHOR_TIME.match(line)
    if match:
      current_blame['time'] = datetime.fromtimestamp(int(match.groups()[0]))
      continue
    match = RE_FILENAME.match(line)
    if match:
      current_blame['filename'] = match.groups()[0]
      current_blame['content'] = next(line_iter).strip()
      continue
  blame_list.append(current_blame)
  return blame_list

RE_MACRO_END = re.compile(r"\);");
RE_DEPRECATE_MACRO = re.compile(r"\(.*?,(.*)\);", re.MULTILINE)

def FilterAndPrint(blame_list, macro, before):
  index = 0
  re_macro = re.compile(macro)
  deprecated = list()
  while index < len(blame_list):
    blame = blame_list[index]
    match = re_macro.search(blame['content'])
    if match and blame['time'] < before:
      line = blame['content']
      time = blame['time']
      pos = match.end()
      start = -1
      parens = 0
      quotes = 0
      while True:
        if pos >= len(line):
          # extend to next line
          index = index + 1
          blame = blame_list[index]
          if line.endswith(','):
            # add whitespace when breaking line due to comma
            line = line + ' '
          line = line + blame['content']
        if line[pos] == '(':
          parens = parens + 1
        elif line[pos] == ')':
          parens = parens - 1
          if parens == 0:
            break
        elif line[pos] == '"':
          quotes = quotes + 1
        elif line[pos] == ',' and quotes % 2 == 0 and start == -1:
          start = pos + 1
        pos = pos + 1
      deprecated.append([index + 1, time, line[start:pos].strip()])
    index = index + 1
  print("Marked as " + macro + ": " + str(len(deprecated)))
  for linenumber, time, content in deprecated:
    print(str(linenumber).rjust(8) + " : " + str(time) + " : " + content)
  return len(deprecated)

def ParseOptions(args):
  parser = argparse.ArgumentParser(description="Collect deprecation statistics")
  parser.add_argument("file_path", help="Path to v8.h")
  parser.add_argument("--before", help="Filter by date")
  options = parser.parse_args(args)
  if options.before:
    options.before = datetime.strptime(options.before, '%Y-%m-%d')
  else:
    options.before = datetime.now()
  return options

def Main(args):
  options = ParseOptions(args)
  blame_list = GetBlame(options.file_path)
  FilterAndPrint(blame_list, "V8_DEPRECATE_SOON", options.before)
  FilterAndPrint(blame_list, "V8_DEPRECATED", options.before)

if __name__ == "__main__":
  Main(sys.argv[1:])
