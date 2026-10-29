#!/usr/bin/env python3
#
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""\
Helper script for compiling and running the Wasm C/C++ API examples.

Usage: tools/run-wasm-api-tests.py outdir tempdir [filters...]

"outdir" is the build output directory containing libwee8, e.g. out/x64.release
"tempdir" is a temporary dir where this script may put its artifacts. It is
the caller's responsibility to clean it up afterwards.

By default, this script builds and runs all examples, both the respective
C and C++ versions, both with GCC ("gcc" and "g++" binaries found in $PATH)
and V8's bundled Clang in third_party/llvm-build/. You can use any number
of "filters" arguments to run only a subset:
 - "c": run C versions of examples
 - "cc": run C++ versions of examples
 - "gcc": compile with GCC
 - "clang": compile with Clang
 - "hello" etc.: run "hello" example
"""

from __future__ import print_function

import os
import shutil
import subprocess
import sys

CFLAGS = "-DDEBUG -Wall -Werror -O0 -ggdb -fsanitize=address"

CHECKOUT_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WASM_PATH = os.path.join(CHECKOUT_PATH, "third_party", "wasm-api")
CLANG_PATH = os.path.join(CHECKOUT_PATH, "third_party", "llvm-build",
                          "Release+Asserts", "bin")

EXAMPLES = ["hello", "callback", "trap", "reflect", "global", "table",
            "memory", "finalize", "serialize", "threads", "hostref", "multi",
            "start"]

CLANG = {
  "name": "Clang",
  "c": os.path.join(CLANG_PATH, "clang"),
  "cc": os.path.join(CLANG_PATH, "clang++"),
  "ldflags": "-fsanitize-memory-track-origins -fsanitize-memory-use-after-dtor",
}
GCC = {
  "name": "GCC",
  "c": "gcc",
  "cc": "g++",
  "ldflags": "",
}

C = {
  "name": "C",
  "suffix": "c",
  "cflags": "",
}
CXX = {
  "name": "C++",
  "suffix": "cc",
  "cflags": "-std=c++11",
}

MIN_ARGS = 3  # Script, outdir, tempdir

def _Call(cmd_list, silent=False):
  cmd = " ".join(cmd_list)
  if not silent: print("# %s" % cmd)
  return subprocess.call(cmd, shell=True)

class Runner(object):
  def __init__(self, name, outdir, tempdir):
    self.name = name
    self.outdir = outdir
    self.tempdir = tempdir
    self.src_file_basename = os.path.join(WASM_PATH, "example", name)
    self.dst_file_basename = os.path.join(tempdir, name)
    self.lib_file = os.path.join(outdir, "obj", "libwee8.a")
    if not os.path.exists(self.lib_file):
      print("libwee8 library not found, make sure to pass the outdir as "
            "first argument; see --help")
      sys.exit(1)
    src_wasm_file = self.src_file_basename + ".wasm"
    dst_wasm_file = self.dst_file_basename + ".wasm"
    shutil.copyfile(src_wasm_file, dst_wasm_file)

  def _Error(self, step, lang, compiler, code):
    print("Error: %s failed. To repro: tools/run-wasm-api-tests.py "
          "%s %s %s %s %s" %
          (step, self.outdir, self.tempdir, self.name, lang,
           compiler["name"].lower()))
    return code

  def CompileAndRun(self, compiler, language):
    print("==== %s %s/%s ====" %
          (self.name, language["name"], compiler["name"]))
    lang = language["suffix"]
    src_file = self.src_file_basename + "." + lang
    exe_file = self.dst_file_basename + "-" + lang
    obj_file = exe_file  + ".o"
    # Compile.
    c = _Call([compiler[lang], "-c", language["cflags"], CFLAGS,
               "-I", WASM_PATH, "-o", obj_file, src_file])
    if c: return self._Error("compilation", lang, compiler, c)
    # Link.
    c = _Call([compiler["cc"], CFLAGS, compiler["ldflags"], obj_file,
               "-o", exe_file, self.lib_file, "-ldl -pthread"])
    if c: return self._Error("linking", lang, compiler, c)
    # Execute.
    exe_file = "./%s-%s" % (self.name, lang)
    c = _Call(["cd", self.tempdir, ";", exe_file])
    if c: return self._Error("execution", lang, compiler, c)
    return 0

def Main(args):
  if (len(args) < MIN_ARGS or args[1] in ("-h", "--help", "help")):
    print(__doc__)
    return 1

  outdir = sys.argv[1]
  tempdir = sys.argv[2]
  result = 0
  examples = EXAMPLES
  compilers = (GCC, CLANG)
  languages = (C, CXX)
  if len(args) > MIN_ARGS:
    custom_compilers = []
    custom_languages = []
    custom_examples = []
    for i in range(MIN_ARGS, len(args)):
      arg = args[i]
      if arg == "c" and C not in custom_languages:
        custom_languages.append(C)
      elif arg in ("cc", "cpp", "cxx", "c++") and CXX not in custom_languages:
        custom_languages.append(CXX)
      elif arg in ("gcc", "g++") and GCC not in custom_compilers:
        custom_compilers.append(GCC)
      elif arg in ("clang", "clang++") and CLANG not in custom_compilers:
        custom_compilers.append(CLANG)
      elif arg in EXAMPLES and arg not in custom_examples:
        custom_examples.append(arg)
      else:
        print("Didn't understand '%s'" % arg)
        return 1
    if custom_compilers:
      compilers = custom_compilers
    if custom_languages:
      languages = custom_languages
    if custom_examples:
      examples = custom_examples
  for example in examples:
    runner = Runner(example, outdir, tempdir)
    for compiler in compilers:
      for language in languages:
        c = runner.CompileAndRun(compiler, language)
        if c: result = c
  if result:
    print("\nFinished with errors.")
  else:
    print("\nFinished successfully.")
  return result

if __name__ == "__main__":
  sys.exit(Main(sys.argv))
