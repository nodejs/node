# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Conversion and renderers for `v8 inspect`."""

import ctypes
import re

from .models import PropertySummary, StructFieldSummary

# Enum values from tools/debug_helper/debug-helper.h.
PROPERTY_KIND_ARRAY_OF_KNOWN_SIZE = 1
PROPERTY_KIND_ARRAY_OF_UNKNOWN_SIZE_INVALID = 2
PROPERTY_KIND_ARRAY_OF_UNKNOWN_SIZE_INACCESSIBLE = 3
TYPE_CHECK_SMI = 0

# TODO(joyee): surface this through the C ABI instead of re-parsing the brief.
_BRIEF_ADDRESS_RE = re.compile(r"0x([0-9a-fA-F]+)\s+<")

# Filler-map names V8 uses to pad unused in-object property slots.
# TODO(joyee): walk the Map's descriptors in GetObjectProperties from debug
# helper to get in-object properties and their names so that we don't need to
# filter this noise from raw backing stores.
_FILLER_BRIEF_MARKERS = ("OnePointerFillerMap", "TwoPointerFillerMap")


def decode_c_str(value, *, lossy=False):
  """Decode a C `char*` (bytes) to str; '' for null/empty.

  `lossy=True` replaces undecodable bytes for untrusted data like briefs.
  """
  if not value:
    return ""
  return value.decode("utf-8", errors="replace" if lossy else "strict")


def extract_brief_address(brief):
  """Return the address parsed from a debug-helper brief, or None."""
  if not brief:
    return None
  matches = _BRIEF_ADDRESS_RE.findall(brief)
  if not matches:
    return None
  return int(matches[-1], 16)


def decode_tagged_smi(raw_value, field_width):
  """Decode the integer value in a tagged Smi."""
  if field_width <= 4:
    return ctypes.c_int32(raw_value).value >> 1
  return ctypes.c_int64(raw_value).value >> 32


_TAGGED_EXACT_TYPES = frozenset({
    "v8::internal::Object",
    "v8::internal::HeapObject",
    "v8::internal::Smi",
})


def _is_tagged_type(type_name):
  """True if `type_name` is one of V8's tagged wrappers."""
  if not type_name:
    return False
  if type_name in _TAGGED_EXACT_TYPES:
    return True
  return (type_name.startswith("v8::internal::Tagged<") or
          type_name.startswith("v8::internal::TaggedMember<"))


def read_frame_trailer(frame_pointer, ptr_size, read_memory):
  """Return (receiver_address, user_argc) for one JS frame, or (None, None)
  if the slots are unreadable. user_argc excludes the receiver slot.
  """
  try:
    # Offsets assume kCPSlotSize == 0 (x64, arm64).
    receiver_bytes = read_memory(frame_pointer + 2 * ptr_size, ptr_size)
    argc_bytes = read_memory(frame_pointer - 3 * ptr_size, ptr_size)
  except Exception:
    return (None, None)
  if len(receiver_bytes) != ptr_size or len(argc_bytes) != ptr_size:
    return (None, None)
  receiver = int.from_bytes(receiver_bytes, "little", signed=False)
  # argc lives in the low 32 bits of the slot.
  raw_argc = int.from_bytes(argc_bytes, "little", signed=False) & 0xFFFFFFFF
  return (receiver, max(0, raw_argc - 1))


def summarize_property(c_prop, uintptr_max):
  """Copy a C ObjectProperty into a Python PropertySummary."""
  type_name = decode_c_str(c_prop.type)
  struct_fields = []
  for i in range(c_prop.num_struct_fields):
    field_ptr = c_prop.struct_fields[i]
    if not field_ptr:
      continue
    try:
      sf = field_ptr.contents
    except Exception:
      continue
    struct_fields.append(
        StructFieldSummary(
            name=decode_c_str(sf.name),
            type=decode_c_str(sf.type),
            offset=int(sf.offset),
            num_bits=int(sf.num_bits),
            shift_bits=int(sf.shift_bits),
        ))
  return PropertySummary(
      name=decode_c_str(c_prop.name),
      type=type_name,
      address=int(c_prop.address) & uintptr_max,
      kind=int(c_prop.kind),
      size=int(c_prop.size),
      num_values=int(c_prop.num_values),
      struct_fields=struct_fields,
      is_tagged=_is_tagged_type(type_name),
  )


def _compact_brief(result):
  """Compact `<Type[: value]>` brief for one tagged value."""
  type_short = (result.type or
                "").removeprefix("v8::internal::") or "HeapObject"
  if result.type_check_result == TYPE_CHECK_SMI:
    m = re.match(r"\s*(-?\d+)", result.brief or "")
    return f"<Smi: {m.group(1)}>" if m else "<Smi>"
  brief = (result.brief or "").strip()
  # debug-helper formats `result.brief` as either `<value> (0x<addr> <type>)`
  # or `0x<addr> <type>`. Pull out the descriptive prefix when present.
  paren = brief.find(" (0x")
  if paren > 0:
    return f"<{type_short}: {brief[:paren]}>"
  return f"<{type_short}>"


class Formatter:
  """Renders an InspectResult tree as head + indented properties.

  Holds the invariants for one `v8 inspect` invocation as instance state; the
  recursion threads only the value being rendered and the current depth.
  """

  def __init__(self, bridge, read_memory, hints, depth=1, array_length=16):
    self._bridge = bridge
    self._read_memory = read_memory
    self._hints = hints
    self._depth = depth
    self._array_length = array_length

  def format(self, result):
    """Render `result`, appending the guessed-types footer at the top level."""
    if result is None:
      return "<?>"
    text = self._render(result, 0)
    if result.guessed_types:
      display = result.display_address or result.address
      lines = [text, "", "could be one of (re-run with --type to drill in):"]
      for guessed in result.guessed_types:
        lines.append(f"  v8 inspect 0x{display:x} --type {guessed}")
      text = "\n".join(lines)
    return text

  def _render(self, result, current_depth):
    """Render one InspectResult, including head line and indented properties."""
    if result is None:
      return "<?>"
    if result.type_check_result == TYPE_CHECK_SMI:
      return _compact_brief(result)
    display = result.display_address or result.address
    head = f"0x{display:x} {_compact_brief(result)}"
    if current_depth >= self._depth or not result.properties:
      return head
    indent = "  " * (current_depth + 1)
    lines = [head]
    for prop in result.properties:
      for line in self._property_lines(prop, current_depth + 1):
        lines.append(indent + line)
    return "\n".join(lines)

  def _property_lines(self, prop, current_depth):
    """Rendered lines for one property without leading indent."""
    kind = prop.kind
    if kind == PROPERTY_KIND_ARRAY_OF_UNKNOWN_SIZE_INVALID:
      return [
          f".{prop.name}=0x{prop.address:x} <length unreadable: invalid memory>"
      ]
    if kind == PROPERTY_KIND_ARRAY_OF_UNKNOWN_SIZE_INACCESSIBLE:
      return [
          f".{prop.name}=0x{prop.address:x} "
          f"<length unreadable: address valid but inaccessible>"
      ]
    if prop.struct_fields:
      return [self._struct(prop)]
    if kind == PROPERTY_KIND_ARRAY_OF_KNOWN_SIZE:
      return self._array_lines(prop, current_depth)
    return [f".{prop.name}=" + self._value(prop, None, current_depth)]

  def _array_lines(self, prop, current_depth):
    """Header plus per-element lines for a kArrayOfKnownSize property, with
    adjacent filler slots collapsed into one summary line each.
    """
    total = prop.num_values
    cap = min(total, self._array_length)
    type_short = (prop.type or "").removeprefix("v8::internal::") or "?"
    lines = [f".{prop.name}=<{type_short}: length={total}>"]
    filler_start = None  # first index of the open filler run, or None.

    def flush_filler(run_end):
      if filler_start is None:
        return
      if filler_start == run_end:
        lines.append(f"  [{filler_start}]=<filler>")
      else:
        count = run_end - filler_start + 1
        lines.append(f"  [{filler_start}..{run_end}]=<filler> ({count} slots)")

    for i in range(cap):
      rendered = self._value(prop, i, current_depth)
      if rendered == "<filler>":
        if filler_start is None:
          filler_start = i
        continue
      flush_filler(i - 1)
      filler_start = None
      value_lines = rendered.split("\n")
      lines.append(f"  [{i}]={value_lines[0]}")
      for cont in value_lines[1:]:
        lines.append("    " + cont)
    flush_filler(cap - 1)
    if total > cap:
      lines.append(f"  ...({total - cap} more)")
    return lines

  def _value(self, prop, element_index, current_depth):
    """Render one value slot. Recurses into tagged children up to `depth`."""
    size = prop.size
    address = (
        prop.address if element_index is None else prop.address +
        element_index * size)
    if not address or not size:
      return "<?>"
    try:
      data = self._read_memory(address, size)
    except Exception:
      return "<?>"
    if len(data) != size:
      return "<?>"
    raw = int.from_bytes(data, "little", signed=False)
    if not prop.is_tagged:
      return f"0x{raw:0{max(2, size * 2)}x}"
    # A Smi-tagged value has its low bit clear.
    if (raw & 1) == 0:
      return f"<Smi: {decode_tagged_smi(raw, size)}>"
    child = self._bridge.inspect(raw, self._hints, self._read_memory)
    if child is None:
      return f"0x{raw:x} <?>"
    if any(m in (child.brief or "") for m in _FILLER_BRIEF_MARKERS):
      return "<filler>"
    return self._render(child, current_depth)

  def _struct(self, prop):
    """Render a Torque-struct field group as raw hex."""
    if not prop.address or not prop.size:
      return f".{prop.name}=<?>"
    try:
      data = self._read_memory(prop.address, prop.size)
    except Exception:
      return f".{prop.name}=<?>"
    if len(data) != prop.size:
      return f".{prop.name}=<?>"
    raw = int.from_bytes(data, "little", signed=False)
    # TODO(joyee): decode packed bitfields and sub-fields inline (e.g. Map::bit_field).
    return f".{prop.name}=0x{raw:0{prop.size * 2}x}"
