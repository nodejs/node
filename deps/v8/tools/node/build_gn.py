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

import argparse
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

BUILD_TARGET = "v8_monolith"

def FindGn(options):
  if options.host_os == "linux":
    os_path = "linux64"
  elif options.host_os == "mac":
    os_path = "mac"
  elif options.host_os == "win":
    os_path = "win"
  else:
    raise "Operating system not supported by GN"
  return os.path.join(options.v8_path, "buildtools", os_path, "gn")

def GenerateBuildFiles(options):
  print "Setting GN args."
  gn = FindGn(options)
  gn_args = []
  gn_args.extend(GN_ARGS)
  for flag in options.flag:
    flag = flag.replace("=1", "=true")
    flag = flag.replace("=0", "=false")
    flag = flag.replace("target_cpu=ia32", "target_cpu=\"x86\"")
    gn_args.append(flag)
  if options.mode == "DEBUG":
    gn_args.append("is_debug = true")
  else:
    gn_args.append("is_debug = false")

  if not os.path.isdir(options.build_path):
    os.makedirs(options.build_path)
  with open(os.path.join(options.build_path, "args.gn"), "w") as args_file:
    args_file.write("\n".join(gn_args))
  subprocess.check_call([gn, "gen", "-C", options.build_path],
                        cwd=options.v8_path)

def Build(options):
  print "Building."
  depot_tools = node_common.EnsureDepotTools(options.v8_path, False)
  ninja = os.path.join(depot_tools, "ninja")
  subprocess.check_call([ninja, "-v", "-C", options.build_path, BUILD_TARGET],
                        cwd=options.v8_path)

def ParseOptions(args):
  parser = argparse.ArgumentParser(
      description="Build %s with GN" % BUILD_TARGET)
  parser.add_argument("--mode", help="Build mode (Release/Debug)")
  parser.add_argument("--v8_path", help="Path to V8")
  parser.add_argument("--build_path", help="Path to build result")
  parser.add_argument("--flag", help="Translate GYP flag to GN",
                      action="append")
  parser.add_argument("--host_os", help="Current operating system")
  options = parser.parse_args(args)

  assert options.host_os
  assert options.mode == "Debug" or options.mode == "Release"

  assert options.v8_path
  options.v8_path = os.path.abspath(options.v8_path)
  assert os.path.isdir(options.v8_path)

  assert options.build_path
  options.build_path = os.path.abspath(options.build_path)
  return options

if __name__ == "__main__":
  options = ParseOptions(sys.argv[1:])
  GenerateBuildFiles(options)
  Build(options)
