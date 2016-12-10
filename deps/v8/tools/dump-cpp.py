#!/usr/bin/env python
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script executes dumpcpp.js, collects all dumped C++ symbols,
# and merges them back into v8 log.

import os
import platform
import re
import subprocess
import sys

def is_file_executable(fPath):
  return os.path.isfile(fPath) and os.access(fPath, os.X_OK)

if __name__ == '__main__':
  JS_FILES = ['splaytree.js', 'codemap.js', 'csvparser.js', 'consarray.js',
              'profile.js', 'logreader.js', 'tickprocessor.js', 'SourceMap.js',
              'dumpcpp.js', 'dumpcpp-driver.js']
  tools_path = os.path.dirname(os.path.realpath(__file__))
  on_windows = platform.system() == 'Windows'
  JS_FILES = [os.path.join(tools_path, f) for f in JS_FILES]

  args = []
  log_file = 'v8.log'
  debug = False
  for arg in sys.argv[1:]:
    if arg == '--debug':
      debug = True
      continue
    args.append(arg)
    if not arg.startswith('-'):
      log_file = arg

  if on_windows:
    args.append('--windows')

  with open(log_file, 'r') as f:
    lines = f.readlines()

  d8_line = re.search(',\"(.*d8)', ''.join(lines))
  if d8_line:
    d8_exec = d8_line.group(1)
    if not is_file_executable(d8_exec):
      print 'd8 binary path found in {} is not executable.'.format(log_file)
      sys.exit(-1)
  else:
    print 'No d8 binary path found in {}.'.format(log_file)
    sys.exit(-1)

  args = [d8_exec] + JS_FILES + ['--'] + args

  with open(log_file) as f:
    sp = subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          stdin=f)
    out, err = sp.communicate()
  if debug:
    print err
  if sp.returncode != 0:
    print out
    exit(-1)

  if on_windows and out:
    out = re.sub('\r+\n', '\n', out)

  is_written = not bool(out)
  with open(log_file, 'w') as f:
    for line in lines:
      if not is_written and line.startswith('tick'):
        f.write(out)
        is_written = True
      f.write(line)
