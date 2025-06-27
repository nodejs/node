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
      """Usage: ./run-gcmole.py [MODE] V8_TARGET_CPU [gcmole.py OPTION]...

Helper script to run gcmole.py on the bots.""")

args = sys.argv[1:]
if "--help" in args:
  print_help()
  exit(0)


# Different modes of running gcmole. Optional to stay backwards-compatible.
mode = 'full'
if args and args[0] in ['check', 'collect', 'full', 'merge']:
  mode = args[0]
  args = args[1:]


if not args:
  print("Missing arguments!")
  print_help()
  exit(1)


if not os.path.isfile("out/build/gen/torque-generated/builtin-definitions.h"):
  print("Expected generated headers in out/build/gen.")
  print("Either build v8 in out/build or change the 'out/build/gen' location in gcmole.py")
  sys.exit(-1)

gcmole_py_options = args[1:]
proc = subprocess.Popen(
    [
        sys.executable,
        GCMOLE_PY,
        mode,
        "--v8-build-dir=%s" % os.path.join(V8_ROOT_DIR, 'out', 'build'),
        "--v8-target-cpu=%s" % args[0],
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
