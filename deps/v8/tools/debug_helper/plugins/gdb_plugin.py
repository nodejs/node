# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GDB integration for the V8 debugger bridge."""

# GDB does not add the directory of the current script to the module
# search path, so we need to do it ourselves to load the shared bridge.
import os
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gdb
from gdb.FrameDecorator import FrameDecorator

from shared_bridge import DebuggerBridge

_VERBOSE = os.environ.get("V8_DEBUG_HELPER_VERBOSE", "") != ""
_bridges = {}


def _get_bridge(ptr_size):
  """Cache one bridge per target pointer size."""
  if ptr_size not in _bridges:
    _bridges[ptr_size] = DebuggerBridge(ptr_size=ptr_size)
  return _bridges[ptr_size]


class V8DbgFrameDecorator(FrameDecorator):
  """Appends V8 JS frame annotations to backtraces."""

  def __init__(self, frame_obj):
    super().__init__(frame_obj)

  def function(self):
    """Keep the native frame name when it is useful, else append JS context."""
    base_name = super().function()
    frame = self.inferior_frame()
    if not base_name:
      try:
        base_name = frame.name() or ""
      except Exception:
        if _VERBOSE:
          traceback.print_exc()
        base_name = ""
    # TODO(joyee): "Builtin" substring check is a coarse heuristic for
    # detecting JS-bridge frames whose native name should not suppress the
    # JS annotation; it can also match unrelated builtins with JS-y names.
    # Refine this when we have a robust way to identify V8 trampoline frames.
    if base_name and "Builtin" not in base_name:
      return base_name
    frame_pointer = 0
    # Extend this register list as the plugin grows support for more
    # architectures. Today the tested targets are x64 and arm64.
    for register_name in ("rbp", "fp", "x29"):
      try:
        value = frame.read_register(register_name)
      except Exception:
        continue
      try:
        frame_pointer = int(value)
      except Exception:
        frame_pointer = 0
      if frame_pointer:
        break
    if not frame_pointer:
      return base_name

    try:
      ptr_size = gdb.lookup_type("void").pointer().sizeof
    except Exception:
      ptr_size = None
    suffix = _get_bridge(ptr_size).frame_suffix(
        frame_pointer, lambda address, byte_count: bytes(gdb.selected_inferior(
        ).read_memory(address, byte_count)))
    if not suffix:
      return base_name
    if not base_name:
      return suffix.strip()
    return f"{base_name}{suffix}"


class V8DbgFrameFilter:
  """Registers the V8 frame decorator."""

  def __init__(self):
    self.name = "v8dbg_bridge"
    self.priority = 100
    self.enabled = True
    progspace = gdb.current_progspace()
    if progspace is not None:
      progspace.frame_filters[self.name] = self
    else:
      gdb.frame_filters[self.name] = self

  def filter(self, iterator):
    """Wrap each frame with the V8-aware decorator."""
    return (V8DbgFrameDecorator(frame_obj) for frame_obj in iterator)


V8DbgFrameFilter()
