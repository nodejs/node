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

from .hints import resolve_current_isolate, resolve_heap_hints
from .inspect import Formatter
from .models import HeapHints

_V8_USAGE = ("usage: v8 <subcommand>\n"
             "  v8 inspect <addr> [--type T] [--depth N] [--array-length N]\n"
             "  v8 isolate\n")

_LIB_PATH_ENV = "V8_DEBUG_HELPER_LIB_PATH"


def debug_helper_lib_warning():
  if os.environ.get(_LIB_PATH_ENV):
    return None
  return (f"v8: {_LIB_PATH_ENV} is not set. The `v8` command and JS frame "
          "annotations will not be registered. Set it to the "
          "libv8_debug_helper shared library and reload this plugin.")


def dispatch_v8_command(bridge, argv, *, read_memory, eval_address, resolver,
                        verbose):
  """Run one `v8 <subcommand> ...` invocation, capturing its output.

  Returns `(True, output)` on success, `(False, error_message)` on failure.
  """
  buffer = io.StringIO()
  if not argv:
    return (True, _V8_USAGE)
  try:
    if argv[0] == "inspect":
      _run_inspect(bridge, argv[1:], buffer, read_memory, eval_address,
                   resolver)
    elif argv[0] == "isolate":
      _run_isolate(argv[1:], buffer, resolver)
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


def _run_isolate(argv, output, resolver):
  """Print the current Isolate of the selected thread."""
  if argv:
    output.write(f"v8 isolate: unexpected argument '{argv[0]}'\n")
    return
  if resolver is None:
    output.write("v8 isolate: no symbol resolver available\n")
    return
  isolate_addr = resolve_current_isolate(resolver)
  if isolate_addr is None:
    output.write(
        "v8 isolate: cannot resolve the current isolate for the selected "
        "thread. Symbols may be missing.\n")
    return
  if not isolate_addr:
    output.write("isolate = <none>\n")
    return
  output.write(f"isolate = 0x{isolate_addr:x}\n")


def _run_inspect(bridge, argv, output, read_memory, eval_address, resolver):
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
  if resolver is not None:
    hints = resolve_heap_hints(resolver)
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
