#!/usr/bin/env python
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# for py2/py3 compatibility
from __future__ import print_function

import os
import pipes
import shutil
import stat
import subprocess
import sys

DEPOT_TOOLS_URL = \
  "https://chromium.googlesource.com/chromium/tools/depot_tools.git"

def EnsureDepotTools(v8_path, fetch_if_not_exist):
  def _Get(v8_path):
    depot_tools = os.path.join(v8_path, "_depot_tools")
    try:
      gclient_path = os.path.join(depot_tools, "gclient.py")
      if os.path.isfile(gclient_path):
        return depot_tools
    except:
      pass
    if fetch_if_not_exist:
      print("Checking out depot_tools.")
      # shell=True needed on Windows to resolve git.bat.
      subprocess.check_call("git clone {} {}".format(
          pipes.quote(DEPOT_TOOLS_URL),
          pipes.quote(depot_tools)), shell=True)
      # Using check_output to hide warning messages.
      subprocess.check_output(
          [sys.executable, gclient_path, "metrics", "--opt-out"],
          cwd=depot_tools)
      return depot_tools
    return None
  depot_tools = _Get(v8_path)
  assert depot_tools is not None
  print("Using depot tools in %s" % depot_tools)
  return depot_tools

def UninitGit(v8_path):
  print("Uninitializing temporary git repository")
  target = os.path.join(v8_path, ".git")
  if os.path.isdir(target):
    print(">> Cleaning up %s" % target)
    def OnRmError(func, path, exec_info):
      # This might happen on Windows
      os.chmod(path, stat.S_IWRITE)
      os.unlink(path)
    shutil.rmtree(target, onerror=OnRmError)
