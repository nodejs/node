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
GCMOLE_PY = os.path.join(GCMOLE_PATH, 'gcmole.py')
V8_ROOT_DIR = os.path.dirname(os.path.dirname(GCMOLE_PATH))


def print_help():
  print(
      """Usage: ./run-gcmole.py TOOLS_GCMOLE_DIR V8_TARGET_CPU [gcmole.py OPTION]...

Helper script to run gcmole.py on the bots.""")


for arg in sys.argv:
  if arg == "--help":
    print_help()
    exit(0)

if len(sys.argv) < 2:
  print("Missing arguments!")
  print_help()
  exit(1)

if not os.path.isfile("out/build/gen/torque-generated/builtin-definitions.h"):
  print("Expected generated headers in out/build/gen.")
  print("Either build v8 in out/build or change the 'out/build/gen' location in gcmole.py")
  sys.exit(-1)

gcmole_py_options = sys.argv[2:]
proc = subprocess.Popen(
    [
        sys.executable,
        GCMOLE_PY,
        "--v8-build-dir=%s" % os.path.join(V8_ROOT_DIR, 'out', 'build'),
        "--v8-target-cpu=%s" % sys.argv[1],
        "--clang-plugins-dir=%s" % CLANG_PLUGINS,
        "--clang-bin-dir=%s" % CLANG_BIN,
        "--is-bot",
    ] + gcmole_py_options,
    cwd=V8_ROOT_DIR,
)

def handle_sigterm(*args):
  try:
    proc.kill()
  except OSError:
    pass

signal.signal(signal.SIGTERM, handle_sigterm)

proc.communicate()
sys.exit(proc.returncode)
