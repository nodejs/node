# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""LLDB integration for the V8 debugger bridge."""

import os
import traceback

import lldb

from shared_bridge import DebuggerBridge

_VERBOSE = os.environ.get("V8_DEBUG_HELPER_VERBOSE", "") != ""
_bridges = {}


def _get_bridge(ptr_size):
  """Cache one bridge per target pointer size."""
  if ptr_size not in _bridges:
    _bridges[ptr_size] = DebuggerBridge(ptr_size=ptr_size)
  return _bridges[ptr_size]


_DEFAULT_FRAME_FORMAT = ("frame #${frame.index}:{ ${frame.no-debug}${frame.pc}}"
                         "{ ${module.file.basename}{`${function.name-with-args}"
                         "{${frame.no-debug}${function.pc-offset}}}}"
                         "{ at ${line.file.basename}:${line.number}}"
                         "{${function.is-optimized} [opt]}")


def frame_annotation(frame, _unused):
  """Return the V8 JS suffix for one LLDB frame, or an empty string."""
  try:
    function_name = frame.GetFunctionName()
    if not function_name:
      symbol = frame.GetSymbol()
      if symbol and symbol.IsValid():
        function_name = symbol.GetName() or ""
    if function_name and "Builtin" not in function_name:
      return ""

    process = frame.GetThread().GetProcess()

    def read_memory(address, byte_count):
      error = lldb.SBError()
      data = process.ReadMemory(address, byte_count, error)
      if not error.Success():
        raise RuntimeError(error.GetCString() or "unable to read memory")
      return data

    fp = frame.GetFP()
    if fp == lldb.LLDB_INVALID_ADDRESS:
      return ""
    ptr_size = process.GetTarget().GetAddressByteSize()
    return _get_bridge(ptr_size).frame_suffix(fp, read_memory)
  except Exception:
    if _VERBOSE:
      traceback.print_exc()
    return ""


def _current_frame_format(debugger):
  """Return the debugger's current `frame-format` setting, if available."""
  result = lldb.SBCommandReturnObject()
  debugger.GetCommandInterpreter().HandleCommand("settings show frame-format",
                                                 result)
  if not result.Succeeded():
    return ""
  return (result.GetOutput() or "").strip()


def __lldb_init_module(debugger, _internal_dict):
  """Hook the LLDB frame format once to annotate V8 frames."""
  callback = f"${{script.frame:{__name__}.frame_annotation}}"
  # We will take over the frame formatting as documented. Warn the user if
  # they had already customised `frame-format` so the takeover is not silent.
  current = _current_frame_format(debugger)
  if current and _DEFAULT_FRAME_FORMAT not in current:
    print("v8 debug helper: overriding existing 'frame-format' setting; "
          "V8 frame annotations will replace your custom format for this "
          "session.")
    if _VERBOSE:
      print(f"v8 debug helper: previous frame-format was: {current}")
  debugger.HandleCommand(
      f"settings set frame-format '{_DEFAULT_FRAME_FORMAT}{callback}\\n'")
