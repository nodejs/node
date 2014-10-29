#!/usr/bin/env python
# Copyright 2014 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import js2c
import os
import re
import sys

FILENAME = "src/runtime/runtime.h"
LISTHEAD = re.compile(r"#define\s+(\w+LIST\w*)\((\w+)\)")
LISTBODY = re.compile(r".*\\$")
BLACKLIST = ['INLINE_FUNCTION_LIST']


class Function(object):
  def __init__(self, match):
    self.name = match.group(1).strip()

def ListMacroRe(list):
  macro = LISTHEAD.match(list[0]).group(2)
  re_string = "\s*%s\((\w+)" % macro
  return re.compile(re_string)


def FindLists(filename):
  lists = []
  current_list = []
  mode = "SEARCHING"
  with open(filename, "r") as f:
    for line in f:
      if mode == "SEARCHING":
        match = LISTHEAD.match(line)
        if match and match.group(1) not in BLACKLIST:
          mode = "APPENDING"
          current_list.append(line)
      else:
        current_list.append(line)
        match = LISTBODY.match(line)
        if not match:
          mode = "SEARCHING"
          lists.append(current_list)
          current_list = []
  return lists


# Detects runtime functions by parsing FILENAME.
def FindRuntimeFunctions():
  functions = []
  lists = FindLists(FILENAME)
  for list in lists:
    function_re = ListMacroRe(list)
    for line in list:
      match = function_re.match(line)
      if match:
        functions.append(Function(match))
  return functions


class Builtin(object):
  def __init__(self, match):
    self.name = match.group(1)


def FindJSNatives():
  PATH = "src"
  fileslist = []
  for (root, dirs, files) in os.walk(PATH):
    for f in files:
      if f.endswith(".js"):
        fileslist.append(os.path.join(root, f))
  natives = []
  regexp = re.compile("^function (\w+)\s*\((.*?)\) {")
  matches = 0
  for filename in fileslist:
    with open(filename, "r") as f:
      file_contents = f.read()
    file_contents = js2c.ExpandInlineMacros(file_contents)
    lines = file_contents.split("\n")
    partial_line = ""
    for line in lines:
      if line.startswith("function") and not '{' in line:
        partial_line += line.rstrip()
        continue
      if partial_line:
        partial_line += " " + line.strip()
        if '{' in line:
          line = partial_line
          partial_line = ""
        else:
          continue
      match = regexp.match(line)
      if match:
        natives.append(Builtin(match))
  return natives


def Main():
  functions = FindRuntimeFunctions()
  natives = FindJSNatives()
  errors = 0
  runtime_map = {}
  for f in functions:
    runtime_map[f.name] = 1
  for b in natives:
    if b.name in runtime_map:
      print("JS_Native/Runtime_Function name clash: %s" % b.name)
      errors += 1

  if errors > 0:
    return 1
  print("Runtime/Natives name clashes: checked %d/%d functions, all good." %
        (len(functions), len(natives)))
  return 0


if __name__ == "__main__":
  sys.exit(Main())
