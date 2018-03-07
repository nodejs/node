#!/usr/bin/env python
# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Use this script to build libv8_monolith.a as dependency for Node.js
Required dependencies can be fetched with fetch_deps.py.

Usage: build_gn.py <Debug/Release> <v8-path> <build-path> [<build-flags>]...

Build flags are passed either as "strings" or numeric value. True/false
are represented as 1/0. E.g.

  v8_promise_internal_field_count=2
  target_cpu="x64"
  v8_enable_disassembler=0
"""

import os
import subprocess
import sys

import node_common

GN_ARGS = [
  "v8_monolithic = true",
  "is_component_build = false",
  "v8_use_external_startup_data = false",
  "use_custom_libcxx = false",
  "use_sysroot = false",
]

BUILD_SUBDIR = "gn"

# TODO: make this cross-platform.
GN_SUBDIR = ["buildtools", "linux64", "gn"]

def Build(v8_path, build_path, depot_tools, is_debug, build_flags):
  print "Setting GN args."
  lines = []
  lines.extend(GN_ARGS)
  for flag in build_flags:
    flag = flag.replace("=1", "=true")
    flag = flag.replace("=0", "=false")
    flag = flag.replace("target_cpu=ia32", "target_cpu=\"x86\"")
    lines.append(flag)
  lines.append("is_debug = %s" % ("true" if is_debug else "false"))
  with open(os.path.join(build_path, "args.gn"), "w") as args_file:
    args_file.write("\n".join(lines))
  gn = os.path.join(v8_path, *GN_SUBDIR)
  subprocess.check_call([gn, "gen", "-C", build_path], cwd=v8_path)
  ninja = os.path.join(depot_tools, "ninja")
  print "Building."
  subprocess.check_call([ninja, "-v", "-C", build_path, "v8_monolith"],
                        cwd=v8_path)

def Main(v8_path, build_path, is_debug, build_flags):
  # Verify paths.
  v8_path = os.path.abspath(v8_path)
  assert os.path.isdir(v8_path)
  build_path = os.path.abspath(build_path)
  build_path = os.path.join(build_path, BUILD_SUBDIR)
  if not os.path.isdir(build_path):
    os.makedirs(build_path)

  # Check that we have depot tools.
  depot_tools = node_common.EnsureDepotTools(v8_path, False)

  # Build with GN.
  Build(v8_path, build_path, depot_tools, is_debug, build_flags)

if __name__ == "__main__":
  # TODO: use argparse to parse arguments.
  build_mode = sys.argv[1]
  v8_path = sys.argv[2]
  build_path = sys.argv[3]
  assert build_mode == "Debug" or build_mode == "Release"
  is_debug = build_mode == "Debug"
  # TODO: introduce "--" flag for pass-through flags.
  build_flags = sys.argv[4:]
  Main(v8_path, build_path, is_debug, build_flags)
