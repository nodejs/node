#!/usr/bin/env python3
# Copyright 2016 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import os.path
import signal
import subprocess
import sys

GCMOLE_PATH = os.path.dirname(os.path.abspath(__file__))
CLANG_BIN = os.path.join(GCMOLE_PATH, 'gcmole-tools', 'bin')
CLANG_PLUGINS = os.path.join(GCMOLE_PATH, 'gcmole-tools')
DRIVER = os.path.join(GCMOLE_PATH, 'gcmole.py')
BASE_PATH = os.path.dirname(os.path.dirname(GCMOLE_PATH))

assert len(sys.argv) >= 2

if not os.path.isfile("out/build/gen/torque-generated/builtin-definitions.h"):
  print("Expected generated headers in out/build/gen.")
  print("Either build v8 in out/build or change the 'out/build/gen' location in gcmole.py")
  sys.exit(-1)

proc = subprocess.Popen(
    [sys.executable, DRIVER] + sys.argv[1:],
    env={'CLANG_BIN': CLANG_BIN, 'CLANG_PLUGINS': CLANG_PLUGINS},
    cwd=BASE_PATH,
)

def handle_sigterm(*args):
  try:
    proc.kill()
  except OSError:
    pass

signal.signal(signal.SIGTERM, handle_sigterm)

proc.communicate()
sys.exit(proc.returncode)
