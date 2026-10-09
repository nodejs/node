# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared implementation of the `v8` debugger command for the GDB/LLDB plugins.

Parses `v8 <subcommand> ...`, resolves the heap hints the debug helper needs,
and renders the result. The bridge is injected by the caller.
"""

import io
import os
import re
import traceback

from .format import (format_frame_location, format_frame_trailer,
                     render_function_span)
from .hints import resolve_current_isolate, resolve_heap_hints
from .inspect import Formatter, preview_tagged_value
from .models import HeapHints

_V8_USAGE = (
    "usage: v8 <subcommand>\n"
    "  v8 inspect <addr>|this|[N] [--type T] [--depth N] [--array-length N]\n"
    "  v8 args [frame#]\n"
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
                   resolver, frame_fp)
    elif argv[0] == "args":
      _run_args(bridge, argv[1:], buffer, read_memory, frame_fp, resolver)
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


def _resolve_js_frame(bridge, command, frame_index, output, read_memory,
                      frame_fp):
  """Resolve a frame number to the matching JS frame metadata.

  Returns `(frame_desc, frame_number, info)`. If the frame cannot be resolved,
  it writes an error and returns None.
  """
  frame_desc = ("the selected frame"
                if frame_index is None else f"frame {frame_index}")
  resolved = frame_fp(frame_index)
  if resolved is None:
    output.write(f"v8 {command}: cannot resolve {frame_desc}\n")
    return None
  frame_number, frame_pointer = resolved
  if frame_number is not None:
    frame_desc = f"frame {frame_number}"

  info = bridge.describe_js_frame(frame_pointer, read_memory)
  if info is None:
    output.write(f"v8 {command}: {frame_desc} is not a JS frame\n")
    return None
  return (frame_desc, frame_number, info)


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

  resolved = _resolve_js_frame(bridge, "source", frame_index, output,
                               read_memory, frame_fp)
  if resolved is None:
    return
  frame_desc, frame_number, info = resolved

  header = format_frame_location(info) + format_frame_trailer(
      info["receiver"], info["argc"])
  if frame_number is not None:
    header = f"#{frame_number}  {header}"
  output.write(header + "\n\n")

  function_name = info.get("function_name") or "<anonymous>"
  span = render_function_span(info, f"{frame_desc}: {function_name}", max_lines)
  if span is None:
    output.write(f"v8 source: no source available for {frame_desc}\n")
    return
  output.write(span)


def _run_args(bridge, argv, output, read_memory, frame_fp, resolver):
  """Print the receiver and the arguments of a JS stack frame."""
  frame_index = None
  for token in argv:
    if token.startswith("-"):
      output.write(f"v8 args: unknown flag '{token}'\n")
      return
    if frame_index is not None:
      output.write(f"v8 args: extra positional arg '{token}'\n")
      return
    if not token.isdigit():
      output.write(f"v8 args: frame number must be a non-negative integer, "
                   f"got '{token}'\n")
      return
    frame_index = int(token)

  resolved = _resolve_js_frame(bridge, "args", frame_index, output, read_memory,
                               frame_fp)
  if resolved is None:
    return
  frame_desc, frame_number, info = resolved

  header = format_frame_location(info)
  if frame_number is not None:
    header = f"#{frame_number}  {header}"
  output.write(header + "\n")

  hints = resolve_heap_hints(resolver) if resolver is not None else HeapHints()
  receiver = info["receiver"]
  if receiver is None:
    output.write("this = <unreadable slot>\n")
  else:
    output.write(
        f"this = {preview_tagged_value(bridge, read_memory, receiver, hints)}\n"
    )
  previews = bridge.argument_previews(info, read_memory, hints=hints)
  if previews is None:
    output.write(f"v8 args: the argument slots of {frame_desc} are "
                 "unreadable\n")
    return
  for index, preview in enumerate(previews):
    output.write(f"[{index}] = {preview}\n")


# Frame-relative value tokens: this and the [N] labels shown in the frame
# annotations.
_FRAME_VALUE_RE = re.compile(r"^(?:this|\[(\d+)\])$")


def _read_argument_slot(info, index, read_memory):
  """Read argument `index` of a frame. Returns `(value, error)`."""
  argc = info["argc"]
  arguments = info["arguments"]
  if argc is None or arguments is None:
    return (None, "the argument slots are unreadable")
  if index >= argc:
    return (None, f"the selected frame has only {argc} argument(s)")
  try:
    data = read_memory(arguments.address + index * arguments.size,
                       arguments.size)
  except Exception:
    data = b""
  if len(data) != arguments.size:
    return (None, f"the [{index}] slot is unreadable")
  return (int.from_bytes(data, "little", signed=False), None)


def _resolve_frame_value(bridge, text, read_memory, frame_fp):
  """Resolve `this`/`[N]` against the selected frame.

  Returns `(handled, value, error)`. `handled` is False when `text` is not a
  frame-relative name. Otherwise `value` holds the tagged slot value, or
  `error` holds the failure message.
  """
  match = _FRAME_VALUE_RE.match(text or "")
  if match is None:
    return (False, None, None)
  resolved = frame_fp(None) if frame_fp is not None else None
  if resolved is None:
    return (True, None, "cannot resolve the selected frame")
  _, frame_pointer = resolved
  info = bridge.describe_js_frame(frame_pointer, read_memory)
  if info is None:
    return (True, None, "the selected frame is not a JS frame")
  if match.group(1) is None:
    receiver = info["receiver"]
    if receiver is None:
      return (True, None, "the receiver slot is unreadable")
    return (True, receiver, None)
  value, error = _read_argument_slot(info, int(match.group(1)), read_memory)
  return (True, value, error)


def _run_inspect(bridge, argv, output, read_memory, eval_address, resolver,
                 frame_fp):
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

  handled, address, error = _resolve_frame_value(bridge, addr_text, read_memory,
                                                 frame_fp)
  if error is not None:
    output.write(f"v8 inspect: {error}\n")
    return
  if not handled:
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
