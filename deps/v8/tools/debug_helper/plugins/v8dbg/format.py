# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Pure text formatting shared by the `v8` command renderers."""


def format_frame_location(annotation):
  """Format "<function_name> @ <script_name>:<line>:<column>" for a JS frame.

  Degrades to "<function_name> @ <script_name>" without a position and to
  "<function_name>" without a script name.
  """
  function_name = annotation.get("function_name") or "<anonymous>"
  script_name = annotation.get("script_name")
  if not script_name:
    return function_name
  location = f"{function_name} @ {script_name}"
  position = annotation.get("position")
  if position:
    location += f":{position[0]}:{position[1]}"
  return location


def format_frame_trailer(receiver, argc):
  """Format the " (this=0x..., argc=N)" frame trailer, or "" without one."""
  if receiver is None:
    return ""
  return f" (this=0x{receiver:x}, argc={argc})"


def render_function_span(info, label, max_lines):
  """Format a function's source span with a bracket rail in the gutter, example:

  14
     +---- frame 5: test_func_3 -----
  15 | function test_func_3(n, b, o, s) {
  16 |   throw new Error("v8dbg bridge test: " + s);
  17 | }
     +---- frame 5: test_func_3 -----

  If there are more than `max_lines` lines, the span is truncated with a note.
  """
  position = info.get("position")
  end_position = info.get("end_position")
  script_source = info.get("script_source") or ""
  lines = script_source.splitlines()

  start_line = position[0] if position else None
  if not lines or start_line is None or start_line > len(lines):
    return None

  rule = f"+---- {label} -----"

  end_line = end_position[0] if end_position else None
  if end_line is not None:
    end_line = min(max(end_line, start_line), len(lines))
  span_last = start_line if end_line is None else end_line
  span_last = min(span_last, start_line + max_lines - 1)

  first = max(1, start_line - 1)
  width = max(4, len(str(span_last)))
  blank_gutter = " " * width

  out = []
  for num in range(first, span_last + 1):
    if num == start_line:
      out.append(f"{blank_gutter} {rule}")
    gutter = "|" if num >= start_line else " "
    out.append(f"{num:>{width}} {gutter} {lines[num - 1]}".rstrip())
  if end_line is not None:
    if end_line > span_last:
      out.append(f"{blank_gutter} |  ... ({end_line - span_last} more lines)")
    else:
      out.append(f"{blank_gutter} {rule}")
  return "\n".join(out) + "\n"
