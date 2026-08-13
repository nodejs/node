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

from .format import (format_frame_location, format_frame_trailer,
                     render_function_span)
from .hints import resolve_current_isolate, resolve_heap_hints
from .inspect import Formatter, read_frame_trailer
from .models import HeapHints

_V8_USAGE = ("usage: v8 <subcommand>\n"
             "  v8 inspect <addr> [--type T] [--depth N] [--array-length N]\n"
             "  v8 isolate\n"
             "  v8 source [frame#] [--max-lines N]\n")

# Cap on the number of function span lines `v8 source` prints. Top-level
# frames span the whole script, which can be arbitrarily large.
_SOURCE_SPAN_MAX_LINES = 10

_LIB_PATH_ENV = "V8_DEBUG_HELPER_LIB_PATH"


def debug_helper_lib_warning():
  if os.environ.get(_LIB_PATH_ENV):
    return None
  return (f"v8: {_LIB_PATH_ENV} is not set. The `v8` command and JS frame "
          "annotations will not be registered. Set it to the "
          "libv8_debug_helper shared library and reload this plugin.")


def dispatch_v8_command(bridge, argv, *, read_memory, eval_address, resolver,
                        verbose, frame_fp):
  """Run one `v8 <subcommand> ...` invocation, capturing its output.

  `frame_fp` maps a frame number (None = the selected frame) to
  `(resolved_number, frame_pointer)`, or None when it cannot be resolved.

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
    elif argv[0] == "source":
      _run_source(bridge, argv[1:], buffer, read_memory, frame_fp)
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


def _run_source(bridge, argv, output, read_memory, frame_fp):
  """Print the source span of the function a stack frame is running."""
  frame_index = None
  max_lines = _SOURCE_SPAN_MAX_LINES
  it = iter(argv)
  for token in it:
    if token == "--max-lines":
      value = next(it, "")
      if not value.isdigit() or int(value) < 1:
        output.write("v8 source: --max-lines must be a positive integer\n")
        return
      max_lines = int(value)
    elif token.startswith("-"):
      output.write(f"v8 source: unknown flag '{token}'\n")
      return
    elif frame_index is not None:
      output.write(f"v8 source: extra positional arg '{token}'\n")
      return
    elif token.isdigit():
      frame_index = int(token)
    else:
      output.write(f"v8 source: frame number must be a non-negative integer, "
                   f"got '{token}'\n")
      return

  frame_desc = ("the selected frame"
                if frame_index is None else f"frame {frame_index}")
  resolved = frame_fp(frame_index)
  if resolved is None:
    output.write(f"v8 source: cannot resolve {frame_desc}\n")
    return
  frame_number, frame_pointer = resolved
  if frame_number is not None:
    frame_desc = f"frame {frame_number}"

  info = bridge.describe_js_frame(frame_pointer, read_memory)
  if info is None:
    output.write(f"v8 source: {frame_desc} is not a JS frame\n")
    return
  receiver, argc = read_frame_trailer(frame_pointer, bridge.ptr_size,
                                      read_memory)

  header = format_frame_location(info) + format_frame_trailer(receiver, argc)
  if frame_number is not None:
    header = f"#{frame_number}  {header}"
  output.write(header + "\n\n")

  function_name = info.get("function_name") or "<anonymous>"
  span = render_function_span(info, f"{frame_desc}: {function_name}", max_lines)
  if span is None:
    output.write(f"v8 source: no source available for {frame_desc}\n")
    return
  output.write(span)


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
