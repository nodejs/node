#!/usr/bin/env python3
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Use this script to fetch all dependencies for V8 to run build_gn.py.

Usage: fetch_deps.py <v8-path>
"""

# for py2/py3 compatibility
from __future__ import print_function

import os
import subprocess
import sys

import node_common

GCLIENT_SOLUTION = [
  { "name"        : "v8",
    "url"         : "https://chromium.googlesource.com/v8/v8.git",
    "deps_file"   : "DEPS",
    "managed"     : False,
    "custom_deps" : {
      # These deps are already part of Node.js.
      "v8/base/trace_event/common"            : None,
      # These deps are unnecessary for building.
      "v8/test/benchmarks/data"               : None,
      "v8/testing/gmock"                      : None,
      "v8/test/mozilla/data"                  : None,
      "v8/test/test262/data"                  : None,
      "v8/test/test262/harness"               : None,
      "v8/third_party/android_ndk"            : None,
      "v8/third_party/android_sdk"            : None,
      "v8/third_party/catapult"               : None,
      "v8/third_party/colorama/src"           : None,
      "v8/third_party/fuchsia-sdk"            : None,
      "v8/third_party/instrumented_libraries" : None,
      "v8/tools/luci-go"                      : None,
      "v8/tools/swarming_client"              : None,
      "v8/third_party/qemu-linux-x64"         : None,
    },
  },
]

def EnsureGit(v8_path):
  def git(args):
    # shell=True needed on Windows to resolve git.bat.
    return subprocess.check_output(
        "git " + args, cwd=v8_path, shell=True).strip()

  expected_git_dir = os.path.join(v8_path, ".git")
  actual_git_dir = git("rev-parse --absolute-git-dir")
  if expected_git_dir == actual_git_dir:
    print("V8 is tracked stand-alone by git.")
    return False
  print("Initializing temporary git repository in v8.")
  git("init")
  git("config user.name \"Ada Lovelace\"")
  git("config user.email ada@lovela.ce")
  git("commit --allow-empty -m init")
  return True

def FetchDeps(v8_path):
  # Verify path.
  v8_path = os.path.abspath(v8_path)
  assert os.path.isdir(v8_path)

  # Check out depot_tools if necessary.
  depot_tools = node_common.EnsureDepotTools(v8_path, True)

  temporary_git = EnsureGit(v8_path)
  try:
    print("Fetching dependencies.")
    env = os.environ.copy()
    # gclient needs to have depot_tools in the PATH.
    env["PATH"] = depot_tools + os.pathsep + env["PATH"]
    gclient = os.path.join(depot_tools, "gclient.py")
    spec = "solutions = %s" % GCLIENT_SOLUTION
    subprocess.check_call([sys.executable, gclient, "sync", "--spec", spec],
                           cwd=os.path.join(v8_path, os.path.pardir),
                           env=env)
  except:
    raise
  finally:
    if temporary_git:
      node_common.UninitGit(v8_path)
    # Clean up .gclient_entries file.
    gclient_entries = os.path.normpath(
        os.path.join(v8_path, os.pardir, ".gclient_entries"))
    if os.path.isfile(gclient_entries):
      os.remove(gclient_entries)

  return depot_tools


if __name__ == "__main__":
  FetchDeps(sys.argv[1])
