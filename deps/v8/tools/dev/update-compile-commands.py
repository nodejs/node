#!/usr/bin/env python3
# Copyright 2020 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""\
Creates a "compile_commands.json" file for V8, for the needs of clangd and
similar code indexers. Also updates generated C++ sources, and compiles the
Torque Language Server, for a complete code indexing experience.
"""

import json
import os
import subprocess
import sys

PYLIB_PATH = 'tools/clang/pylib'
GM_PATH = 'tools/dev'
PYLIB_CHECK = os.path.join(PYLIB_PATH, 'clang', 'compile_db.py')
GM_CHECK = os.path.join(GM_PATH, 'gm.py')
def CheckRelativeImport(path):
  if not os.path.exists(path):
    print("Error: Please run this script from the root of a V8 checkout. %s "
          "must be a valid relative path." % path)
    sys.exit(1)
CheckRelativeImport(PYLIB_CHECK)
CheckRelativeImport(GM_CHECK)

sys.path.insert(0, PYLIB_PATH)
from clang import compile_db

sys.path.insert(0, GM_PATH)
import gm

def _Call(cmd, silent=False):
  if not silent: print("# %s" % cmd)
  return subprocess.call(cmd, shell=True)

def _Write(filename, content):
  with open(filename, "w") as f:
    f.write(content)

def PrepareBuildDir(arch, mode):
  build_dir = os.path.join("out", "%s.%s" % (arch, mode))
  if not os.path.exists(build_dir):
    print("# mkdir -p %s" % build_dir)
    os.makedirs(build_dir)
  args_gn = os.path.join(build_dir, "args.gn")
  if not os.path.exists(args_gn):
    conf = gm.Config(arch, mode, [])
    _Write(args_gn, conf.GetGnArgs())
  build_ninja = os.path.join(build_dir, "build.ninja")
  if not os.path.exists(build_ninja):
    code = _Call("gn gen %s" % build_dir)
    if code != 0: raise Exception("gn gen failed")
  else:
    _Call("ninja -C %s build.ninja" % build_dir)
  return build_dir

def AddTargetsForArch(arch, combined):
  build_dir = PrepareBuildDir(arch, "debug")
  commands = compile_db.ProcessCompileDatabase(
                compile_db.GenerateWithNinja(build_dir, ["all"]), [])
  added = 0
  for c in commands:
    key = c["file"]
    if key not in combined:
      combined[key] = c
      added += 1
  print("%s: added %d compile commands" % (arch, added))

def UpdateCompileCommands():
  print(">>> Updating compile_commands.json...")
  combined = {}
  AddTargetsForArch("x64", combined)
  AddTargetsForArch("ia32", combined)
  AddTargetsForArch("arm", combined)
  AddTargetsForArch("arm64", combined)
  commands = []
  for key in combined:
    commands.append(combined[key])
  _Write("compile_commands.json", json.dumps(commands, indent=2))

def CompileLanguageServer():
  print(">>> Compiling Torque Language Server...")
  PrepareBuildDir("x64", "release")
  _Call("autoninja -C out/x64.release torque-language-server")

def GenerateCCFiles():
  print(">>> Generating generated C++ source files...")
  # This must be called after UpdateCompileCommands().
  assert os.path.exists("out/x64.debug/build.ninja")
  _Call("autoninja -C out/x64.debug v8_generated_cc_files")

def StartGoma():
  gomadir = gm.DetectGoma()
  if (gomadir is not None and
      _Call("ps -e | grep compiler_proxy > /dev/null", silent=True) != 0):
    _Call("%s/goma_ctl.py ensure_start" % gomadir)

if __name__ == "__main__":
  StartGoma()
  CompileLanguageServer()
  UpdateCompileCommands()
  GenerateCCFiles()
