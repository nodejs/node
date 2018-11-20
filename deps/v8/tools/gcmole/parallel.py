#!/usr/bin/env python
# Copyright 2015 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
This script calls the first argument for each of the following arguments in
parallel. E.g.
parallel.py "clang --opt" file1 file2
calls
clang --opt file1
clang --opt file2

The output (stdout and stderr) is concatenated sequentially in the form:
______________ file1
<output of clang --opt file1>
______________ finish <exit code of clang --opt file1> ______________
______________ file2
<output of clang --opt file2>
______________ finish <exit code of clang --opt file2> ______________
"""

import itertools
import multiprocessing
import subprocess
import sys

def invoke(cmdline):
  try:
    return (subprocess.check_output(
        cmdline, shell=True, stderr=subprocess.STDOUT), 0)
  except subprocess.CalledProcessError as e:
    return (e.output, e.returncode)

if __name__ == '__main__':
  assert len(sys.argv) > 2
  processes = multiprocessing.cpu_count()
  pool = multiprocessing.Pool(processes=processes)
  cmdlines = ["%s %s" % (sys.argv[1], filename) for filename in sys.argv[2:]]
  for filename, result in itertools.izip(
      sys.argv[2:], pool.imap(invoke, cmdlines)):
    print "______________ %s" % filename
    print result[0]
    print "______________ finish %d ______________" % result[1]
