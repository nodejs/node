# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import lldb
import re

def jst(debugger, *args):
  """Print the current JavaScript stack trace"""
  target = debugger.GetSelectedTarget()
  process = target.GetProcess()
  thread = process.GetSelectedThread()
  frame = thread.GetSelectedFrame()
  frame.EvaluateExpression("_v8_internal_Print_StackTrace();")
  print("")

def jss(debugger, *args):
  """Skip the jitted stack on x64 to where we entered JS last"""
  target = debugger.GetSelectedTarget()
  process = target.GetProcess()
  thread = process.GetSelectedThread()
  frame = thread.GetSelectedFrame()
  js_entry_sp = frame.EvaluateExpression(
      "v8::internal::Isolate::Current()->thread_local_top()->js_entry_sp_;") \
       .GetValue()
  sizeof_void = frame.EvaluateExpression("sizeof(void*)").GetValue()
  rbp = frame.FindRegister("rbp")
  rsp = frame.FindRegister("rsp")
  pc = frame.FindRegister("pc")
  rbp = js_entry_sp
  rsp = js_entry_sp + 2 *sizeof_void
  pc.value = js_entry_sp + sizeof_void

def bta(debugger, *args):
  """Print stack trace with assertion scopes"""
  func_name_re = re.compile("([^(<]+)(?:\(.+\))?")
  assert_re = re.compile(
      "^v8::internal::Per\w+AssertType::(\w+)_ASSERT, (false|true)>")
  target = debugger.GetSelectedTarget()
  process = target.GetProcess()
  thread = process.GetSelectedThread()
  frame = thread.GetSelectedFrame()
  for frame in thread:
    functionSignature = frame.GetDisplayFunctionName()
    if functionSignature is None:
      continue
    functionName = func_name_re.match(functionSignature)
    line = frame.GetLineEntry().GetLine()
    sourceFile = frame.GetLineEntry().GetFileSpec().GetFilename()
    if line:
      sourceFile = sourceFile + ":" + str(line)

    if sourceFile is None:
      sourceFile = ""
    print("[%-2s] %-60s %-40s" % (frame.GetFrameID(),
                                  functionName.group(1),
                                  sourceFile))
    match = assert_re.match(str(functionSignature))
    if match:
      if match.group(3) == "false":
        prefix = "Disallow"
        color = "\033[91m"
      else:
        prefix = "Allow"
        color = "\033[92m"
      print("%s -> %s %s (%s)\033[0m" % (
          color, prefix, match.group(2), match.group(1)))

def __lldb_init_module (debugger, dict):
  debugger.HandleCommand('command script add -f lldb_commands.jst jst')
  debugger.HandleCommand('command script add -f lldb_commands.jss jss')
  debugger.HandleCommand('command script add -f lldb_commands.bta bta')
