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
  "v8_monolithic=true",
  "is_component_build=false",
  "v8_use_external_startup_data=false",
  "use_custom_libcxx=false",
]

BUILD_TARGET = "v8_monolith"

def FindTargetOs(flags):
  for flag in flags:
    if flag.startswith("target_os="):
      return flag[len("target_os="):].strip('"')
  raise Exception('No target_os was set.')

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
  gn = FindGn(options)
  gn_args = list(GN_ARGS)
  target_os = FindTargetOs(options.flag)
  if target_os != "win":
    gn_args.append("use_sysroot=false")

  for flag in options.flag:
    flag = flag.replace("=1", "=true")
    flag = flag.replace("=0", "=false")
    flag = flag.replace("target_cpu=ia32", "target_cpu=\"x86\"")
    gn_args.append(flag)
  if options.mode == "Debug":
    gn_args.append("is_debug=true")
  else:
    gn_args.append("is_debug=false")

  flattened_args = ' '.join(gn_args)
  if options.extra_gn_args:
    flattened_args += ' ' + options.extra_gn_args

  args = [gn, "gen", options.build_path, "-q", "--args=" + flattened_args]
  subprocess.check_call(args)

def Build(options):
  depot_tools = node_common.EnsureDepotTools(options.v8_path, False)
  ninja = os.path.join(depot_tools, "ninja")
  if sys.platform == 'win32':
    # Required because there is an extension-less file called "ninja".
    ninja += ".exe"
  args = [ninja, "-C", options.build_path, BUILD_TARGET]
  if options.max_load:
    args += ["-l" + options.max_load]
  if options.max_jobs:
    args += ["-j" + options.max_jobs]
  else:
    with open(os.path.join(options.build_path, "args.gn")) as f:
      if "use_goma = true" in f.read():
        args += ["-j500"]
  subprocess.check_call(args)

def ParseOptions(args):
  parser = argparse.ArgumentParser(
      description="Build %s with GN" % BUILD_TARGET)
  parser.add_argument("--mode", help="Build mode (Release/Debug)")
  parser.add_argument("--v8_path", help="Path to V8", required=True)
  parser.add_argument("--build_path", help="Path to build result",
                      required=True)
  parser.add_argument("--flag", help="Translate GYP flag to GN",
                      action="append")
  parser.add_argument("--host_os", help="Current operating system")
  parser.add_argument("--bundled-win-toolchain",
                      help="Value for DEPOT_TOOLS_WIN_TOOLCHAIN")
  parser.add_argument("--bundled-win-toolchain-root",
                      help="Value for DEPOT_TOOLS_WIN_TOOLCHAIN_ROOT")
  parser.add_argument("--depot-tools", help="Absolute path to depot_tools")
  parser.add_argument("--extra-gn-args", help="Additional GN args")
  parser.add_argument("--build", help="Run ninja as opposed to gn gen.",
                      action="store_true")
  parser.add_argument("--max-jobs", help="ninja's -j parameter")
  parser.add_argument("--max-load", help="ninja's -l parameter")
  options = parser.parse_args(args)

  options.build_path = os.path.abspath(options.build_path)

  if not options.build:
    assert options.host_os
    assert options.mode == "Debug" or options.mode == "Release"

    options.v8_path = os.path.abspath(options.v8_path)
    assert os.path.isdir(options.v8_path)

  return options


if __name__ == "__main__":
  options = ParseOptions(sys.argv[1:])
  # Build can result in running gn gen, so need to set environment variables
  # for build as well as generate.
  if options.bundled_win_toolchain:
    os.environ['DEPOT_TOOLS_WIN_TOOLCHAIN'] = options.bundled_win_toolchain
  if options.bundled_win_toolchain_root:
    os.environ['DEPOT_TOOLS_WIN_TOOLCHAIN_ROOT'] = (
        options.bundled_win_toolchain_root)
  if options.depot_tools:
    os.environ['PATH'] = (
        options.depot_tools + os.path.pathsep + os.environ['PATH'])
  if not options.build:
    GenerateBuildFiles(options)
  else:
    Build(options)
