# Copyright 2017 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Load this file by adding this to your ~/.lldbinit:
# command script import <this_dir>/lldb_commands.py

# for py2/py3 compatibility
from __future__ import print_function

import lldb
import re

#####################
# Helper functions. #
#####################
def current_thread(debugger):
  return debugger.GetSelectedTarget().GetProcess().GetSelectedThread()

def current_frame(debugger):
  return current_thread(debugger).GetSelectedFrame()

def no_arg_cmd(debugger, cmd):
  evaluate_result = current_frame(debugger).EvaluateExpression(cmd)
  # When a void function is called the return value type is 0x1001 which
  # is specified in http://tiny.cc/bigskz. This does not indicate
  # an error so we check for that value below.
  kNoResult = 0x1001
  error = evaluate_result.GetError()
  if error.fail and error.value != kNoResult:
      print("Failed to evaluate command {} :".format(cmd))
      print(error.description)
  else:
    print("")

def ptr_arg_cmd(debugger, name, param, cmd):
  if not param:
    print("'{}' requires an argument".format(name))
    return
  param = '(void*)({})'.format(param)
  no_arg_cmd(debugger, cmd.format(param))

#####################
# lldb commands.    #
#####################
def job(debugger, param, *args):
  """Print a v8 heap object"""
  ptr_arg_cmd(debugger, 'job', param, "_v8_internal_Print_Object({})")

def jlh(debugger, param, *args):
  """Print v8::Local handle value"""
  ptr_arg_cmd(debugger, 'jlh', param,
              "_v8_internal_Print_Object(*(v8::internal::Object**)({}.val_))")

def jco(debugger, param, *args):
  """Print the code object at the given pc (default: current pc)"""
  if not param:
    param = str(current_frame(debugger).FindRegister("pc").value)
  ptr_arg_cmd(debugger, 'jco', param, "_v8_internal_Print_Code({})")

def jld(debugger, param, *args):
  """Print a v8 LayoutDescriptor object"""
  ptr_arg_cmd(debugger, 'jld', param,
              "_v8_internal_Print_LayoutDescriptor({})")

def jtt(debugger, param, *args):
  """Print the transition tree of a v8 Map"""
  ptr_arg_cmd(debugger, 'jtt', param, "_v8_internal_Print_TransitionTree({})")

def jst(debugger, *args):
  """Print the current JavaScript stack trace"""
  no_arg_cmd(debugger, "_v8_internal_Print_StackTrace()")

def jss(debugger, *args):
  """Skip the jitted stack on x64 to where we entered JS last"""
  frame = current_frame(debugger)
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
  thread = current_thread(debugger)
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

def __lldb_init_module(debugger, dict):
  debugger.HandleCommand('settings set target.x86-disassembly-flavor intel')
  for cmd in ('job', 'jlh', 'jco', 'jld', 'jtt', 'jst', 'jss', 'bta'):
    debugger.HandleCommand(
      'command script add -f lldb_commands.{} {}'.format(cmd, cmd))
