# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared implementation of the `v8` debugger command for the GDB/LLDB plugins.

Parses `v8 <subcommand> ...`, resolves the heap hints the debug helper needs,
and renders the result. The bridge is injected by the caller.
"""

import io
import os
import traceback

from .inspect import Formatter
from .models import HeapHints

_V8_USAGE = (
    "usage: v8 inspect <addr> [--type T] [--depth N] [--array-length N]\n")

_LIB_PATH_ENV = "V8_DEBUG_HELPER_LIB_PATH"


def debug_helper_lib_warning():
  if os.environ.get(_LIB_PATH_ENV):
    return None
  return (f"v8: {_LIB_PATH_ENV} is not set. The `v8` command and JS frame "
          "annotations will not be registered. Set it to the "
          "libv8_debug_helper shared library and reload this plugin.")


def dispatch_v8_command(bridge, argv, *, read_memory, eval_address,
                        hints_resolver, verbose):
  """Run one `v8 <subcommand> ...` invocation, capturing its output.

  Returns `(True, output)` on success, `(False, error_message)` on failure.
  """
  buffer = io.StringIO()
  if not argv:
    return (True, _V8_USAGE)
  try:
    if argv[0] == "inspect":
      _run_inspect(bridge, argv[1:], buffer, read_memory, eval_address,
                   hints_resolver)
    else:
      return (True, f"v8: unknown subcommand '{argv[0]}'\n{_V8_USAGE}")
  except Exception:
    if verbose:
      traceback.print_exc()
      return (False, "v8: command failed")
    return (False,
            "v8: command failed. See more info with V8_DEBUG_HELPER_VERBOSE=1")
  return (True, buffer.getvalue())


def _parse_address(text, eval_address):
  """Parse `<addr>` text from the CLI. Fall back to debugger eval."""
  s = (text or "").strip()
  if not s:
    return None
  try:
    if s.lower().startswith("0x"):
      return int(s, 16)
    return int(s, 10)
  except ValueError:
    pass
  if eval_address is not None:
    try:
      result = eval_address(s)
      if result is not None:
        return int(result)
    except Exception:
      return None
  return None


def resolve_heap_hints(resolver):
  """Resolve V8 isolate symbols and offsets through `resolver` into HeapHints."""
  hints = HeapHints()
  sym_addr = resolver.global_symbol_address("v8::internal::g_current_isolate_")
  isolate_addr = resolver.read_pointer(sym_addr) if sym_addr else None
  if not isolate_addr:
    return hints

  # v8::internal::IsolateGroup::metadata_pointer_table_, reached via Isolate::isolate_group_.
  group_offset = resolver.field_offset("v8::internal::Isolate",
                                       "isolate_group_")
  if group_offset is not None:
    group_ptr = resolver.read_pointer(isolate_addr + group_offset)
    mpt_offset = resolver.field_offset("v8::internal::IsolateGroup",
                                       "metadata_pointer_table_")
    if group_ptr and mpt_offset is not None:
      hints.metadata_pointer_table = group_ptr + mpt_offset

  # The offset of the heap_ member within the Isolate class.
  heap_isolate_offset = resolver.field_offset("v8::internal::Heap", "isolate_")
  if heap_isolate_offset is not None:
    hints.isolate_heap_member_offset = int(heap_isolate_offset)

  # any_heap_pointer is filled in by `_run_inspect` from the CLI address.
  return hints


def _run_inspect(bridge, argv, output, read_memory, eval_address,
                 hints_resolver):
  # TODO(joyee): per-flag error messages, reject negative ints.
  type_hint = None
  depth = 1
  array_length = 16
  addr_text = None
  it = iter(argv)
  for token in it:
    if token == "--type":
      type_hint = next(it)
    elif token == "--depth":
      depth = int(next(it))
    elif token == "--array-length":
      array_length = int(next(it))
    elif token.startswith("-"):
      output.write(f"v8 inspect: unknown flag '{token}'\n")
      return
    elif addr_text is None:
      addr_text = token
    else:
      output.write(f"v8 inspect: extra positional arg '{token}'\n")
      return

  address = _parse_address(addr_text, eval_address)
  if address is None:
    output.write(_V8_USAGE)
    return

  hints = HeapHints()
  if hints_resolver is not None:
    hints = resolve_heap_hints(hints_resolver)
  if not hints.any_heap_pointer:
    hints.any_heap_pointer = address

  result = bridge.inspect(address, hints, read_memory, type_hint=type_hint)
  if result is None:
    output.write(f"v8 inspect: no result for 0x{address:x}\n")
    return
  output.write(
      Formatter(
          bridge, read_memory, hints, depth=depth,
          array_length=array_length).format(result))
  output.write("\n")
