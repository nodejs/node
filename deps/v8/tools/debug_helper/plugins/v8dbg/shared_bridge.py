# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared ctypes bridge for the v8_debug_helper C ABI.

This module loads libv8_debug_helper and exposes frame annotation
and inspection helpers used by the GDB and LLDB plugins.
"""

import ctypes
import os
import re
import types
from typing import Optional

from .inspect import (
    decode_c_str,
    decode_tagged_smi,
    extract_brief_address,
    read_frame_trailer,
    summarize_property,
)
from .models import HeapHints, InspectResult

_MEMORY_ACCESS_OK = 0
_MEMORY_ACCESS_INVALID = 1

_STRING_LITERAL_RE = re.compile(r'"(?:[^"\\]|\\.)*"')

# Default per-field string length cap for frame annotations. Use None if unlimited.
_MAX_PROP_STRING_CHARS = 256


class StructProperty(ctypes.Structure):
  _fields_ = [
      ("name", ctypes.c_char_p),
      ("type", ctypes.c_char_p),
      ("offset", ctypes.c_size_t),
      ("num_bits", ctypes.c_uint8),
      ("shift_bits", ctypes.c_uint8),
  ]


StructPropertyPointer = ctypes.POINTER(StructProperty)


def _make_types(ptr_size):
  """Create ctypes types parameterized by the target pointer size (4 or 8)."""
  c_uintptr = ctypes.c_uint64 if ptr_size == 8 else ctypes.c_uint32
  uintptr_max = (1 << (ptr_size * 8)) - 1

  class ObjectProperty(ctypes.Structure):
    _fields_ = [
        ("name", ctypes.c_char_p),
        ("type", ctypes.c_char_p),
        ("address", c_uintptr),
        ("num_values", ctypes.c_size_t),
        ("size", ctypes.c_size_t),
        ("num_struct_fields", ctypes.c_size_t),
        ("struct_fields", ctypes.POINTER(StructPropertyPointer)),
        ("kind", ctypes.c_int),
    ]

  ObjectPropertyPointer = ctypes.POINTER(ObjectProperty)

  class ObjectPropertiesResult(ctypes.Structure):
    _fields_ = [
        ("type_check_result", ctypes.c_int),
        ("brief", ctypes.c_char_p),
        ("type", ctypes.c_char_p),
        ("num_properties", ctypes.c_size_t),
        ("properties", ctypes.POINTER(ObjectPropertyPointer)),
        ("num_guessed_types", ctypes.c_size_t),
        ("guessed_types", ctypes.POINTER(ctypes.c_char_p)),
    ]

  class StackFrameResult(ctypes.Structure):
    _fields_ = [
        ("num_properties", ctypes.c_size_t),
        ("properties", ctypes.POINTER(ObjectPropertyPointer)),
    ]

  class HeapAddresses(ctypes.Structure):
    _fields_ = [
        ("map_space_first_page", c_uintptr),
        ("old_space_first_page", c_uintptr),
        ("read_only_space_first_page", c_uintptr),
        ("any_heap_pointer", c_uintptr),
        ("metadata_pointer_table", c_uintptr),
        ("isolate_heap_member_offset", c_uintptr),
    ]

  MemoryAccessor = ctypes.CFUNCTYPE(ctypes.c_int, c_uintptr, ctypes.c_void_p,
                                    ctypes.c_size_t)

  return types.SimpleNamespace(
      c_uintptr=c_uintptr,
      uintptr_max=uintptr_max,
      ObjectProperty=ObjectProperty,
      ObjectPropertyPointer=ObjectPropertyPointer,
      ObjectPropertiesResult=ObjectPropertiesResult,
      StackFrameResult=StackFrameResult,
      HeapAddresses=HeapAddresses,
      MemoryAccessor=MemoryAccessor,
  )


# TODO(joyee): add a method to check that the library is compatible with the binary
# being debugged e.g. the V8 build configs should match.
class DebuggerBridge:

  def __init__(self, library_path=None, ptr_size=None):
    """Set up a lazily loaded bridge for one target pointer width."""
    self._library_path = library_path
    self._library_handle = None
    if ptr_size is None:
      ptr_size = ctypes.sizeof(ctypes.c_void_p)
    self._ptr_size = ptr_size
    self._t = _make_types(ptr_size)

  def _resolved_library_path(self):
    """Resolve the debug-helper shared library from config or environment."""
    lib_path = self._library_path or os.environ.get("V8_DEBUG_HELPER_LIB_PATH")
    if not lib_path:
      raise RuntimeError(
          "Set V8_DEBUG_HELPER_LIB_PATH to the v8_debug_helper shared library")
    return os.path.abspath(lib_path)

  def _library(self):
    """Load and type the C ABI once for this bridge instance."""
    if self._library_handle is None:
      library = ctypes.CDLL(self._resolved_library_path())
      t = self._t
      library._v8_debug_helper_GetStackFrame.argtypes = [
          t.c_uintptr,
          t.MemoryAccessor,
      ]
      library._v8_debug_helper_GetStackFrame.restype = ctypes.POINTER(
          t.StackFrameResult)
      library._v8_debug_helper_Free_StackFrameResult.argtypes = [
          ctypes.POINTER(t.StackFrameResult)
      ]
      library._v8_debug_helper_Free_StackFrameResult.restype = None
      library._v8_debug_helper_GetObjectProperties.argtypes = [
          t.c_uintptr,
          t.MemoryAccessor,
          ctypes.POINTER(t.HeapAddresses),
          ctypes.c_char_p,
      ]
      library._v8_debug_helper_GetObjectProperties.restype = ctypes.POINTER(
          t.ObjectPropertiesResult)
      library._v8_debug_helper_Free_ObjectPropertiesResult.argtypes = [
          ctypes.POINTER(t.ObjectPropertiesResult)
      ]
      library._v8_debug_helper_Free_ObjectPropertiesResult.restype = None
      self._library_handle = library
    return self._library_handle

  def _make_memory_accessor(self, read_memory):
    """Adapt a debugger-specific memory reader to the C callback ABI."""

    def callback(address, destination, byte_count):
      try:
        data = read_memory(address, byte_count)
      except Exception:
        return _MEMORY_ACCESS_INVALID
      if len(data) != byte_count:
        return _MEMORY_ACCESS_INVALID
      ctypes.memmove(destination, data, byte_count)
      return _MEMORY_ACCESS_OK

    return self._t.MemoryAccessor(callback)

  def _hints_to_c(self, hints):
    """Convert a HeapHints dataclass into the C HeapAddresses struct."""
    return self._t.HeapAddresses(
        int(hints.map_space_first_page) & self._t.uintptr_max,
        int(hints.old_space_first_page) & self._t.uintptr_max,
        int(hints.read_only_space_first_page) & self._t.uintptr_max,
        int(hints.any_heap_pointer) & self._t.uintptr_max,
        int(hints.metadata_pointer_table) & self._t.uintptr_max,
        int(hints.isolate_heap_member_offset) & self._t.uintptr_max,
    )

  @property
  def ptr_size(self):
    """Target pointer size in bytes (4 or 8)."""
    return self._ptr_size

  def inspect(self, tagged_ptr, heap_hints, read_memory, type_hint=None):
    """Wrap _v8_debug_helper_GetObjectProperties and return an InspectResult.

    Returns None only if the C call fails outright. Partial data conditions
    surface via `type_check_result`.
    """
    if heap_hints is None:
      heap_hints = HeapHints()
    memory_callback = self._make_memory_accessor(read_memory)
    c_hints = self._hints_to_c(heap_hints)
    library = self._library()
    type_hint_bytes = (
        type_hint.encode("utf-8") if isinstance(type_hint, str) else type_hint)
    tagged = int(tagged_ptr) & self._t.uintptr_max
    result_ptr = library._v8_debug_helper_GetObjectProperties(
        tagged, memory_callback, ctypes.byref(c_hints), type_hint_bytes)
    if not result_ptr:
      return None
    try:
      result = result_ptr.contents
      brief = decode_c_str(result.brief, lossy=True)
      type_name = decode_c_str(result.type)
      properties = [
          summarize_property(result.properties[i].contents, self._t.uintptr_max)
          for i in range(result.num_properties)
      ]
      guessed_types = [
          decode_c_str(result.guessed_types[i], lossy=True)
          for i in range(result.num_guessed_types)
          if result.guessed_types[i]
      ]
      return InspectResult(
          brief=brief,
          type=type_name,
          type_check_result=int(result.type_check_result),
          properties=properties,
          guessed_types=guessed_types,
          address=tagged,
          display_address=(extract_brief_address(brief) or 0),
      )
    finally:
      library._v8_debug_helper_Free_ObjectPropertiesResult(result_ptr)

  def _extract_string_from_brief(self, brief):
    """Extract string content from object briefs."""
    if not brief:
      return ""
    match = _STRING_LITERAL_RE.search(brief)
    if match:
      return match.group(0)[1:-1]
    marker = " (0x"
    if marker in brief:
      return brief.rsplit(marker, 1)[0]
    return brief

  def _resolve_property_string(
      self,
      props,
      prop_name,
      read_memory,
      allow_brief_fallback=True,
      max_chars: Optional[int] = _MAX_PROP_STRING_CHARS):
    """Resolve one debug-helper property into a Python string when possible."""
    prop = props.get(prop_name)
    if prop is None or not prop.address or not prop.size:
      return ""

    raw_value = int.from_bytes(
        read_memory(prop.address, prop.size), byteorder="little", signed=False)

    hints = HeapHints(any_heap_pointer=int(prop.address))
    result = self.inspect(raw_value, hints, read_memory)
    if result is None:
      return ""
    for p in result.properties:
      # TODO(joyee): repeatedly decoding the same string can be expensive,
      # but for live debugging stale caches can be tricky to manage. Find
      # a way to tie the lifetime of these caches correctly with debugger
      # action cycles so that we can speed up repeated reads.
      if p.name in ("chars", "raw_characters") and p.num_values > 0:
        size_per_char = p.size  # 1 for char, 2 for char16_t
        chars_to_read = p.num_values
        if max_chars is not None:
          chars_to_read = min(chars_to_read, max_chars)
        try:
          data = read_memory(p.address, chars_to_read * size_per_char)
          if size_per_char == 1:
            text = data.decode("latin-1")
          else:
            text = data.decode("utf-16-le")
        except Exception:
          break
        if max_chars is not None and p.num_values > chars_to_read:
          text += "..."
        return text
    if not allow_brief_fallback:
      return ""
    # Fall back to the display-oriented brief only for non-source strings.
    return self._extract_string_from_brief(result.brief or "")

  def _position_from_offset(self, script_source, char_offset):
    """Convert a source character offset to a user-facing line and column."""
    if not script_source or not 0 <= char_offset <= len(script_source):
      return None

    # TODO(joyee): we likely want to cache this for repeated lookups within
    # the same script, but for live debugging we need to be careful to
    # invalidate the cache correctly.
    line = script_source.count("\n", 0, char_offset) + 1
    last_newline = script_source.rfind("\n", 0, char_offset)
    column = (
        char_offset + 1 if last_newline == -1 else char_offset - last_newline)
    return (line, column)

  def _decode_position(self, script_source, function_name, offset_prop,
                       read_memory):
    """Decode function_character_offset into a (line, column) pair."""
    if (offset_prop is None or not offset_prop.address or
        not offset_prop.struct_fields):
      return None

    fields = {f.name: f for f in offset_prop.struct_fields}
    start_field = fields.get("start")
    end_field = fields.get("end")
    if start_field is None or end_field is None:
      return None

    # The debug helper reports the total struct size as 2 * kTaggedSize for the
    # target build, so derive the per-field width from that build-specific size.
    if start_field.offset != 0 or offset_prop.size % 2 != 0:
      return None
    field_width = offset_prop.size // 2
    if field_width not in (4, 8):
      return None
    raw_start = int.from_bytes(
        read_memory(offset_prop.address + start_field.offset, field_width),
        byteorder="little",
        signed=False,
    )
    char_offset = decode_tagged_smi(raw_start, field_width)
    position = self._position_from_offset(script_source, char_offset)
    if position is not None:
      return position

    # Top-level scripts without source can still be reported as 1:1.
    if not script_source and char_offset == 0 and function_name == "":
      return (1, 1)
    return None

  def describe_js_frame(self, frame_pointer, read_memory):
    """Return high-level JS frame metadata, or None if the frame is unusable."""
    if not frame_pointer:
      return None

    memory_callback = self._make_memory_accessor(read_memory)
    library = self._library()
    result_ptr = library._v8_debug_helper_GetStackFrame(
        int(frame_pointer) & self._t.uintptr_max, memory_callback)
    if not result_ptr:
      return None
    try:
      result = result_ptr.contents
      props = {}
      for index in range(result.num_properties):
        summary = summarize_property(result.properties[index].contents,
                                     self._t.uintptr_max)
        props[summary.name] = summary
      if "currently_executing_jsfunction" not in props:
        return None

      function_name = self._resolve_property_string(props, "function_name",
                                                    read_memory)
      script_name = self._resolve_property_string(
          props, "script_name", read_memory, max_chars=None)
      script_source = self._resolve_property_string(
          props,
          "script_source",
          read_memory,
          allow_brief_fallback=False,
          max_chars=None)
      position = self._decode_position(
          script_source,
          function_name,
          props.get("function_character_offset"),
          read_memory,
      )

      if function_name == "" and not script_name:
        return None
      if function_name == "":
        function_name = "<anonymous>"
      return {
          "function_name": function_name,
          "script_name": script_name,
          "position": position,
      }
    finally:
      library._v8_debug_helper_Free_StackFrameResult(result_ptr)

  def frame_suffix(self, frame_pointer, read_memory):
    """Format the JS annotation suffix for a debugger frame:

    [<function_name> @ <script_name>:<line>:<column>] (this=0x..., argc=N)

    If source text cannot be recovered but the script name still can, the
    annotation degrades to:

    [<function_name> @ <script_name>]

    Drops the trailing `(this=..., argc=...)` if the frame slots are unreadable.
    """
    annotation = self.describe_js_frame(frame_pointer, read_memory)
    if not annotation:
      return ""
    function_name = annotation.get("function_name") or "<anonymous>"
    script_name = annotation.get("script_name")
    position = annotation.get("position")
    location_suffix = ""
    if position:
      location_suffix = f":{position[0]}:{position[1]}"
    if script_name:
      head = f" [{function_name} @ {script_name}{location_suffix}]"
    else:
      head = f" [{function_name}]"
    receiver, argc = read_frame_trailer(frame_pointer, self._ptr_size,
                                        read_memory)
    if receiver is None:
      return head
    return f"{head} (this=0x{receiver:x}, argc={argc})"


_bridges = {}


def get_bridge(ptr_size):
  """Return a cached DebuggerBridge for `ptr_size` (one per target width)."""
  if ptr_size not in _bridges:
    _bridges[ptr_size] = DebuggerBridge(ptr_size=ptr_size)
  return _bridges[ptr_size]
