#!/usr/bin/env python3
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


"""
Launcher for the foozzie differential-fuzzing harness. Wraps foozzie
with Python2 for backwards-compatibility when bisecting.

Obsolete now after switching to Python3 entirely. We keep the launcher
for a transition period.
"""

import os
import re
import subprocess
import sys

if __name__ == '__main__':
  # In some cases or older versions, the python executable is passed as
  # first argument. Let's be robust either way, with or without full
  # path or version.
  if re.match(r'.*python.*', sys.argv[1]):
    args = sys.argv[2:]
  else:
    args = sys.argv[1:]
  process = subprocess.Popen(['python3'] + args)
  process = subprocess.Popen(args)
  process.communicate()
  sys.exit(process.returncode)
