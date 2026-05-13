# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Shared ctypes bridge for the v8_debug_helper C ABI.

This module loads libv8_debug_helper and exposes frame annotation
helpers used by the GDB and LLDB plugins.
"""

import ctypes
import os
import re
import types

_MEMORY_ACCESS_OK = 0
_MEMORY_ACCESS_INVALID = 1

_STRING_LITERAL_RE = re.compile(r'"(?:[^"\\]|\\.)*"')


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

  def _resolve_property_string(self,
                               props,
                               prop_name,
                               read_memory,
                               allow_brief_fallback=True):
    """Resolve one debug-helper property into a Python string when possible."""
    prop = props.get(prop_name)
    if prop is None or not prop.address or not prop.size:
      return ""

    raw_value = int.from_bytes(
        read_memory(prop.address, prop.size), byteorder="little", signed=False)

    memory_callback = self._make_memory_accessor(read_memory)
    # `any_heap_pointer` is used by the debug helper to locate the V8 cage /
    # read-only space. We pass `prop.address` here, which is the address of
    # the parent object's *field* rather than a heap object pointer, but it
    # still lies within the isolate's heap and is therefore good enough for
    # cage detection.
    heap_addresses = self._t.HeapAddresses(
        0,
        0,
        0,
        int(prop.address) & self._t.uintptr_max,
        0,
        0,
    )
    library = self._library()
    result_ptr = library._v8_debug_helper_GetObjectProperties(
        int(raw_value) & self._t.uintptr_max,
        memory_callback,
        ctypes.byref(heap_addresses),
        None,
    )
    if not result_ptr:
      return ""
    try:
      result = result_ptr.contents
      # TODO(joyee): repeatedly decoding the same string can be expensive,
      # but for live debugging stale caches can be tricky to manage. Find
      # a way to tie the lifetime of these caches correctly with debugger
      # action cycles so that we can speed up repeated reads.
      for i in range(result.num_properties):
        p = result.properties[i].contents
        name = p.name.decode("utf-8")
        if name in ("chars", "raw_characters") and p.num_values > 0:
          size_per_char = p.size  # 1 for char, 2 for char16_t
          try:
            data = read_memory(p.address, p.num_values * size_per_char)
            if size_per_char == 1:
              return data.decode("latin-1")
            return data.decode("utf-16-le")
          except Exception:
            break
      if not allow_brief_fallback:
        return ""
      # Fall back to the display-oriented brief only for non-source strings.
      brief = ""
      if result.brief:
        brief = result.brief.decode("utf-8", errors="replace")
      return self._extract_string_from_brief(brief)
    finally:
      library._v8_debug_helper_Free_ObjectPropertiesResult(result_ptr)

  def _decode_tagged_smi(self, raw_value, field_width):
    """Decode one tagged Smi value read from target memory."""
    if field_width <= 4:
      return ctypes.c_int32(raw_value).value >> 1
    return ctypes.c_int64(raw_value).value >> 32

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
        offset_prop.num_struct_fields == 0):
      return None

    fields = {}
    # Convert the struct fields array into a dict for easier lookup.
    for index in range(offset_prop.num_struct_fields):
      field = offset_prop.struct_fields[index].contents
      fields[field.name.decode("utf-8")] = field
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
    char_offset = self._decode_tagged_smi(raw_start, field_width)
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
      # Convert the properties array into a dict for easier lookup.
      for index in range(result.num_properties):
        prop = result.properties[index].contents
        props[prop.name.decode("utf-8")] = prop
      if "currently_executing_jsfunction" not in props:
        return None

      function_name = self._resolve_property_string(props, "function_name",
                                                    read_memory)
      script_name = self._resolve_property_string(props, "script_name",
                                                  read_memory)
      script_source = self._resolve_property_string(
          props, "script_source", read_memory, allow_brief_fallback=False)
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
    """Format one bracket-wrapped JS annotation suffix for a debugger frame:

    [<function_name> @ <script_name>:<line>:<column>]

    If source text cannot be recovered but the script name still can, the
    annotation degrades to:

    [<function_name> @ <script_name>]
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
      return f" [{function_name} @ {script_name}{location_suffix}]"
    return f" [{function_name}]"
