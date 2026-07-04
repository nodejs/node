# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""GDB integration for the V8 debugger bridge."""

# GDB does not add the directory of the current script to the module
# search path, so we need to do it ourselves to load the v8dbg/ package.
import os
import sys
import traceback

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gdb
from gdb.FrameDecorator import FrameDecorator

from v8dbg.commands import debug_helper_lib_warning
from v8dbg.commands import dispatch_v8_command
from v8dbg.shared_bridge import get_bridge

_VERBOSE = os.environ.get("V8_DEBUG_HELPER_VERBOSE", "") != ""


def _read_memory_callback(address, byte_count):
  """Read target memory through the currently selected inferior."""
  return bytes(gdb.selected_inferior().read_memory(address, byte_count))


def _ptr_size():
  """Best-effort target pointer size."""
  try:
    return gdb.lookup_type("void").pointer().sizeof
  except Exception:
    return None


class _GdbHintsResolver:
  """GDB Adapter for resolve_heap_hints."""

  def __init__(self, ptr_size):
    self._ptr_size = ptr_size

  def read_pointer(self, addr):
    try:
      data = bytes(gdb.selected_inferior().read_memory(addr, self._ptr_size))
    except Exception:
      return None
    if len(data) != self._ptr_size:
      return None
    return int.from_bytes(data, "little", signed=False)

  def global_symbol_address(self, name):
    try:
      sym = gdb.lookup_global_symbol(name)
      if sym is None:
        return None
      value = sym.value()
      addr = value.address
      if addr is None:
        # Try `&name` via parse_and_eval, which resolves TLS for the selected thread.
        addr = gdb.parse_and_eval(f"&'{name}'")
      if addr is None:
        return None
      return int(addr)
    except Exception:
      return None

  def field_offset(self, type_name, field_name):
    """Return the byte offset of `field_name` inside `type_name`."""
    try:
      t = gdb.lookup_type(type_name)
    except Exception:
      return None
    try:
      field = t[field_name]
    except Exception:
      return None
    # https://sourceware.org/gdb/current/onlinedocs/gdb.html/Types-In-Python.html
    bitpos = getattr(field, "bitpos", None)
    if bitpos is None:
      return None
    return int(bitpos) // 8


def _evaluate_address_in_gdb(text):
  """Evaluate a debugger expression as an integer address."""
  try:
    return int(gdb.parse_and_eval(text))
  except Exception:
    return None


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
    # On non-pointer-compressed builds with short builtin calls enabled, the
    # builtins are remapped and no longer sit at the in-binary address range
    # registered in the symbol table, so gdb cannot symbolize them and renders
    # them as '???'. Strip the placeholder so an annotated frame does not read
    # '??? [func @ ...]'.
    if base_name == "???":
      base_name = ""
    # This is a heuristic to early skip frames that clearly not builtin frames.
    # We only try to symbolicate frames that are either not symbolicated, or have a
    # name that starts with "Builtins_". The call frame_suffix() below will check
    # if it's a genuine JS frame.
    if base_name and not base_name.startswith("Builtins_"):
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

    # frame_suffix() consults debug_helper, which returns a non-empty
    # annotation only for genuine JS frames, otherwise it returns an empty string.
    bridge = get_bridge(_ptr_size())
    suffix = bridge.frame_suffix(frame_pointer, _read_memory_callback)
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


class V8Command(gdb.Command):
  """V8 debug helpers.
  Syntax: v8 <subcommand> [options]

  Available subcommands:

    - inspect -- Inspect the heap object at <addr> with the V8 inspector.

      v8 inspect <addr> [--type T] [--depth N] [--array-length N]

      * --type T: treat the object as if it has type T (e.g. `v8::internal::JSArray`).
      * --depth N: limit recursive inspection depth to N (default: 1).
      * --array-length N: for arrays, inspect up to N elements (default: 16).
  """

  def __init__(self):
    super().__init__("v8", gdb.COMMAND_USER, prefix=False)

  def invoke(self, argument, from_tty):
    """Dispatch the command and stream output back to the gdb console."""
    try:
      argv = gdb.string_to_argv(argument)
    except gdb.error:
      argv = argument.split()
    bridge = get_bridge(_ptr_size())
    ok, text = dispatch_v8_command(
        bridge,
        argv,
        read_memory=_read_memory_callback,
        eval_address=_evaluate_address_in_gdb,
        hints_resolver=_GdbHintsResolver(bridge.ptr_size),
        verbose=_VERBOSE,
    )
    if not ok:
      gdb.write(text + "\n", gdb.STDERR)
    elif text:
      gdb.write(text)


_lib_warning = debug_helper_lib_warning()
if _lib_warning is not None:
  gdb.write(_lib_warning + "\n", gdb.STDERR)
else:
  V8DbgFrameFilter()
  V8Command()
