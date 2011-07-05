#!/usr/bin/env python
#
# Copyright 2011 the V8 project authors. All rights reserved.
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of Google Inc. nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import ctypes
import mmap
import optparse
import os
import disasm
import sys
import types
import codecs
import re


USAGE="""usage: %prog [OPTION]...

Minidump analyzer.

Shows the processor state at the point of exception including the
stack of the active thread and the referenced objects in the V8
heap. Code objects are disassembled and the addresses linked from the
stack (pushed return addresses) are marked with "=>".


Examples:
  $ %prog 12345678-1234-1234-1234-123456789abcd-full.dmp
"""

DEBUG=False


def DebugPrint(s):
  if not DEBUG: return
  print s


class Descriptor(object):
  """Descriptor of a structure in a memory."""

  def __init__(self, fields):
    self.fields = fields
    self.is_flexible = False
    for _, type_or_func in fields:
      if isinstance(type_or_func, types.FunctionType):
        self.is_flexible = True
        break
    if not self.is_flexible:
      self.ctype = Descriptor._GetCtype(fields)
      self.size = ctypes.sizeof(self.ctype)

  def Read(self, memory, offset):
    if self.is_flexible:
      fields_copy = self.fields[:]
      last = 0
      for name, type_or_func in fields_copy:
        if isinstance(type_or_func, types.FunctionType):
          partial_ctype = Descriptor._GetCtype(fields_copy[:last])
          partial_object = partial_ctype.from_buffer(memory, offset)
          type = type_or_func(partial_object)
          if type is not None:
            fields_copy[last] = (name, type)
            last += 1
        else:
          last += 1
      complete_ctype = Descriptor._GetCtype(fields_copy[:last])
    else:
      complete_ctype = self.ctype
    return complete_ctype.from_buffer(memory, offset)

  @staticmethod
  def _GetCtype(fields):
    class Raw(ctypes.Structure):
      _fields_ = fields
      _pack_ = 1

      def __str__(self):
        return "{" + ", ".join("%s: %s" % (field, self.__getattribute__(field))
                               for field, _ in Raw._fields_) + "}"
    return Raw


# Set of structures and constants that describe the layout of minidump
# files. Based on MSDN and Google Breakpad.

MINIDUMP_HEADER = Descriptor([
  ("signature", ctypes.c_uint32),
  ("version", ctypes.c_uint32),
  ("stream_count", ctypes.c_uint32),
  ("stream_directories_rva", ctypes.c_uint32),
  ("checksum", ctypes.c_uint32),
  ("time_date_stampt", ctypes.c_uint32),
  ("flags", ctypes.c_uint64)
])

MINIDUMP_LOCATION_DESCRIPTOR = Descriptor([
  ("data_size", ctypes.c_uint32),
  ("rva", ctypes.c_uint32)
])

MINIDUMP_DIRECTORY = Descriptor([
  ("stream_type", ctypes.c_uint32),
  ("location", MINIDUMP_LOCATION_DESCRIPTOR.ctype)
])

MD_EXCEPTION_MAXIMUM_PARAMETERS = 15

MINIDUMP_EXCEPTION = Descriptor([
  ("code", ctypes.c_uint32),
  ("flags", ctypes.c_uint32),
  ("record", ctypes.c_uint64),
  ("address", ctypes.c_uint64),
  ("parameter_count", ctypes.c_uint32),
  ("unused_alignment", ctypes.c_uint32),
  ("information", ctypes.c_uint64 * MD_EXCEPTION_MAXIMUM_PARAMETERS)
])

MINIDUMP_EXCEPTION_STREAM = Descriptor([
  ("thread_id", ctypes.c_uint32),
  ("unused_alignment", ctypes.c_uint32),
  ("exception", MINIDUMP_EXCEPTION.ctype),
  ("thread_context", MINIDUMP_LOCATION_DESCRIPTOR.ctype)
])

# Stream types.
MD_UNUSED_STREAM = 0
MD_RESERVED_STREAM_0 = 1
MD_RESERVED_STREAM_1 = 2
MD_THREAD_LIST_STREAM = 3
MD_MODULE_LIST_STREAM = 4
MD_MEMORY_LIST_STREAM = 5
MD_EXCEPTION_STREAM = 6
MD_SYSTEM_INFO_STREAM = 7
MD_THREAD_EX_LIST_STREAM = 8
MD_MEMORY_64_LIST_STREAM = 9
MD_COMMENT_STREAM_A = 10
MD_COMMENT_STREAM_W = 11
MD_HANDLE_DATA_STREAM = 12
MD_FUNCTION_TABLE_STREAM = 13
MD_UNLOADED_MODULE_LIST_STREAM = 14
MD_MISC_INFO_STREAM = 15
MD_MEMORY_INFO_LIST_STREAM = 16
MD_THREAD_INFO_LIST_STREAM = 17
MD_HANDLE_OPERATION_LIST_STREAM = 18

MD_FLOATINGSAVEAREA_X86_REGISTERAREA_SIZE = 80

MINIDUMP_FLOATING_SAVE_AREA_X86 = Descriptor([
  ("control_word", ctypes.c_uint32),
  ("status_word", ctypes.c_uint32),
  ("tag_word", ctypes.c_uint32),
  ("error_offset", ctypes.c_uint32),
  ("error_selector", ctypes.c_uint32),
  ("data_offset", ctypes.c_uint32),
  ("data_selector", ctypes.c_uint32),
  ("register_area", ctypes.c_uint8 * MD_FLOATINGSAVEAREA_X86_REGISTERAREA_SIZE),
  ("cr0_npx_state", ctypes.c_uint32)
])

MD_CONTEXT_X86_EXTENDED_REGISTERS_SIZE = 512

# Context flags.
MD_CONTEXT_X86 = 0x00010000
MD_CONTEXT_X86_CONTROL = (MD_CONTEXT_X86 | 0x00000001)
MD_CONTEXT_X86_INTEGER = (MD_CONTEXT_X86 | 0x00000002)
MD_CONTEXT_X86_SEGMENTS = (MD_CONTEXT_X86 | 0x00000004)
MD_CONTEXT_X86_FLOATING_POINT = (MD_CONTEXT_X86 | 0x00000008)
MD_CONTEXT_X86_DEBUG_REGISTERS = (MD_CONTEXT_X86 | 0x00000010)
MD_CONTEXT_X86_EXTENDED_REGISTERS = (MD_CONTEXT_X86 | 0x00000020)

def EnableOnFlag(type, flag):
  return lambda o: [None, type][int((o.context_flags & flag) != 0)]

MINIDUMP_CONTEXT_X86 = Descriptor([
  ("context_flags", ctypes.c_uint32),
  # MD_CONTEXT_X86_DEBUG_REGISTERS.
  ("dr0", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_DEBUG_REGISTERS)),
  ("dr1", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_DEBUG_REGISTERS)),
  ("dr2", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_DEBUG_REGISTERS)),
  ("dr3", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_DEBUG_REGISTERS)),
  ("dr6", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_DEBUG_REGISTERS)),
  ("dr7", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_DEBUG_REGISTERS)),
  # MD_CONTEXT_X86_FLOATING_POINT.
  ("float_save", EnableOnFlag(MINIDUMP_FLOATING_SAVE_AREA_X86.ctype,
                              MD_CONTEXT_X86_FLOATING_POINT)),
  # MD_CONTEXT_X86_SEGMENTS.
  ("gs", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_SEGMENTS)),
  ("fs", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_SEGMENTS)),
  ("es", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_SEGMENTS)),
  ("ds", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_SEGMENTS)),
  # MD_CONTEXT_X86_INTEGER.
  ("edi", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_INTEGER)),
  ("esi", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_INTEGER)),
  ("ebx", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_INTEGER)),
  ("edx", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_INTEGER)),
  ("ecx", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_INTEGER)),
  ("eax", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_INTEGER)),
  # MD_CONTEXT_X86_CONTROL.
  ("ebp", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_CONTROL)),
  ("eip", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_CONTROL)),
  ("cs", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_CONTROL)),
  ("eflags", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_CONTROL)),
  ("esp", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_CONTROL)),
  ("ss", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_X86_CONTROL)),
  # MD_CONTEXT_X86_EXTENDED_REGISTERS.
  ("extended_registers",
   EnableOnFlag(ctypes.c_uint8 * MD_CONTEXT_X86_EXTENDED_REGISTERS_SIZE,
                MD_CONTEXT_X86_EXTENDED_REGISTERS))
])

MINIDUMP_MEMORY_DESCRIPTOR = Descriptor([
  ("start", ctypes.c_uint64),
  ("memory", MINIDUMP_LOCATION_DESCRIPTOR.ctype)
])

MINIDUMP_MEMORY_DESCRIPTOR64 = Descriptor([
  ("start", ctypes.c_uint64),
  ("size", ctypes.c_uint64)
])

MINIDUMP_MEMORY_LIST = Descriptor([
  ("range_count", ctypes.c_uint32),
  ("ranges", lambda m: MINIDUMP_MEMORY_DESCRIPTOR.ctype * m.range_count)
])

MINIDUMP_MEMORY_LIST64 = Descriptor([
  ("range_count", ctypes.c_uint64),
  ("base_rva", ctypes.c_uint64),
  ("ranges", lambda m: MINIDUMP_MEMORY_DESCRIPTOR64.ctype * m.range_count)
])

MINIDUMP_THREAD = Descriptor([
  ("id", ctypes.c_uint32),
  ("suspend_count", ctypes.c_uint32),
  ("priority_class", ctypes.c_uint32),
  ("priority", ctypes.c_uint32),
  ("ted", ctypes.c_uint64),
  ("stack", MINIDUMP_MEMORY_DESCRIPTOR.ctype),
  ("context", MINIDUMP_LOCATION_DESCRIPTOR.ctype)
])

MINIDUMP_THREAD_LIST = Descriptor([
  ("thread_count", ctypes.c_uint32),
  ("threads", lambda t: MINIDUMP_THREAD.ctype * t.thread_count)
])


class MinidumpReader(object):
  """Minidump (.dmp) reader."""

  _HEADER_MAGIC = 0x504d444d

  def __init__(self, options, minidump_name):
    self.minidump_name = minidump_name
    self.minidump_file = open(minidump_name, "r")
    self.minidump = mmap.mmap(self.minidump_file.fileno(), 0, mmap.MAP_PRIVATE)
    self.header = MINIDUMP_HEADER.Read(self.minidump, 0)
    if self.header.signature != MinidumpReader._HEADER_MAGIC:
      print >>sys.stderr, "Warning: unsupported minidump header magic"
    DebugPrint(self.header)
    directories = []
    offset = self.header.stream_directories_rva
    for _ in xrange(self.header.stream_count):
      directories.append(MINIDUMP_DIRECTORY.Read(self.minidump, offset))
      offset += MINIDUMP_DIRECTORY.size
    self.exception = None
    self.exception_context = None
    self.memory_list = None
    self.thread_map = {}
    for d in directories:
      DebugPrint(d)
      # TODO(vitalyr): extract system info including CPU features.
      if d.stream_type == MD_EXCEPTION_STREAM:
        self.exception = MINIDUMP_EXCEPTION_STREAM.Read(
          self.minidump, d.location.rva)
        DebugPrint(self.exception)
        self.exception_context = MINIDUMP_CONTEXT_X86.Read(
          self.minidump, self.exception.thread_context.rva)
        DebugPrint(self.exception_context)
      elif d.stream_type == MD_THREAD_LIST_STREAM:
        thread_list = MINIDUMP_THREAD_LIST.Read(self.minidump, d.location.rva)
        assert ctypes.sizeof(thread_list) == d.location.data_size
        DebugPrint(thread_list)
        for thread in thread_list.threads:
          DebugPrint(thread)
          self.thread_map[thread.id] = thread
      elif d.stream_type == MD_MEMORY_LIST_STREAM:
        print >>sys.stderr, "Warning: not a full minidump"
        ml = MINIDUMP_MEMORY_LIST.Read(self.minidump, d.location.rva)
        DebugPrint(ml)
        for m in ml.ranges:
          DebugPrint(m)
      elif d.stream_type == MD_MEMORY_64_LIST_STREAM:
        assert self.memory_list is None
        self.memory_list = MINIDUMP_MEMORY_LIST64.Read(
          self.minidump, d.location.rva)
        assert ctypes.sizeof(self.memory_list) == d.location.data_size
        DebugPrint(self.memory_list)

  def IsValidAddress(self, address):
    return self.FindLocation(address) is not None

  def ReadU8(self, address):
    location = self.FindLocation(address)
    return ctypes.c_uint8.from_buffer(self.minidump, location).value

  def ReadU32(self, address):
    location = self.FindLocation(address)
    return ctypes.c_uint32.from_buffer(self.minidump, location).value

  def ReadBytes(self, address, size):
    location = self.FindLocation(address)
    return self.minidump[location:location + size]

  def FindLocation(self, address):
    # TODO(vitalyr): only works for full minidumps (...64 structure variants).
    offset = 0
    for r in self.memory_list.ranges:
      if r.start <= address < r.start + r.size:
        return self.memory_list.base_rva + offset + address - r.start
      offset += r.size
    return None

  def GetDisasmLines(self, address, size):
    location = self.FindLocation(address)
    if location is None: return []
    return disasm.GetDisasmLines(self.minidump_name,
                                 location,
                                 size,
                                 "ia32",
                                 False)


  def Dispose(self):
    self.minidump.close()
    self.minidump_file.close()


# List of V8 instance types. Obtained by adding the code below to any .cc file.
#
# #define DUMP_TYPE(T) printf("%d: \"%s\",\n", T, #T);
# struct P {
#   P() {
#     printf("{\n");
#     INSTANCE_TYPE_LIST(DUMP_TYPE)
#     printf("}\n");
#   }
# };
# static P p;
INSTANCE_TYPES = {
  64: "SYMBOL_TYPE",
  68: "ASCII_SYMBOL_TYPE",
  65: "CONS_SYMBOL_TYPE",
  69: "CONS_ASCII_SYMBOL_TYPE",
  66: "EXTERNAL_SYMBOL_TYPE",
  74: "EXTERNAL_SYMBOL_WITH_ASCII_DATA_TYPE",
  70: "EXTERNAL_ASCII_SYMBOL_TYPE",
  0: "STRING_TYPE",
  4: "ASCII_STRING_TYPE",
  1: "CONS_STRING_TYPE",
  5: "CONS_ASCII_STRING_TYPE",
  2: "EXTERNAL_STRING_TYPE",
  10: "EXTERNAL_STRING_WITH_ASCII_DATA_TYPE",
  6: "EXTERNAL_ASCII_STRING_TYPE",
  6: "PRIVATE_EXTERNAL_ASCII_STRING_TYPE",
  128: "MAP_TYPE",
  129: "CODE_TYPE",
  130: "ODDBALL_TYPE",
  131: "JS_GLOBAL_PROPERTY_CELL_TYPE",
  132: "HEAP_NUMBER_TYPE",
  133: "PROXY_TYPE",
  134: "BYTE_ARRAY_TYPE",
  135: "PIXEL_ARRAY_TYPE",
  136: "EXTERNAL_BYTE_ARRAY_TYPE",
  137: "EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE",
  138: "EXTERNAL_SHORT_ARRAY_TYPE",
  139: "EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE",
  140: "EXTERNAL_INT_ARRAY_TYPE",
  141: "EXTERNAL_UNSIGNED_INT_ARRAY_TYPE",
  142: "EXTERNAL_FLOAT_ARRAY_TYPE",
  143: "FILLER_TYPE",
  144: "ACCESSOR_INFO_TYPE",
  145: "ACCESS_CHECK_INFO_TYPE",
  146: "INTERCEPTOR_INFO_TYPE",
  147: "CALL_HANDLER_INFO_TYPE",
  148: "FUNCTION_TEMPLATE_INFO_TYPE",
  149: "OBJECT_TEMPLATE_INFO_TYPE",
  150: "SIGNATURE_INFO_TYPE",
  151: "TYPE_SWITCH_INFO_TYPE",
  152: "SCRIPT_TYPE",
  153: "CODE_CACHE_TYPE",
  156: "FIXED_ARRAY_TYPE",
  157: "SHARED_FUNCTION_INFO_TYPE",
  158: "JS_MESSAGE_OBJECT_TYPE",
  159: "JS_VALUE_TYPE",
  160: "JS_OBJECT_TYPE",
  161: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  162: "JS_GLOBAL_OBJECT_TYPE",
  163: "JS_BUILTINS_OBJECT_TYPE",
  164: "JS_GLOBAL_PROXY_TYPE",
  165: "JS_ARRAY_TYPE",
  166: "JS_REGEXP_TYPE",
  167: "JS_FUNCTION_TYPE",
  154: "DEBUG_INFO_TYPE",
  155: "BREAK_POINT_INFO_TYPE",
}


class Printer(object):
  """Printer with indentation support."""

  def __init__(self):
    self.indent = 0

  def Indent(self):
    self.indent += 2

  def Dedent(self):
    self.indent -= 2

  def Print(self, string):
    print "%s%s" % (self._IndentString(), string)

  def PrintLines(self, lines):
    indent = self._IndentString()
    print "\n".join("%s%s" % (indent, line) for line in lines)

  def _IndentString(self):
    return self.indent * " "


ADDRESS_RE = re.compile(r"0x[0-9a-fA-F]+")


def FormatDisasmLine(start, heap, line):
  line_address = start + line[0]
  stack_slot = heap.stack_map.get(line_address)
  marker = "  "
  if stack_slot:
    marker = "=>"
  code = AnnotateAddresses(heap, line[1])
  return "%s%08x %08x: %s" % (marker, line_address, line[0], code)


def AnnotateAddresses(heap, line):
  extra = []
  for m in ADDRESS_RE.finditer(line):
    maybe_address = int(m.group(0), 16)
    object = heap.FindObject(maybe_address)
    if not object: continue
    extra.append(str(object))
  if len(extra) == 0: return line
  return "%s  ;; %s" % (line, ", ".join(extra))


class HeapObject(object):
  def __init__(self, heap, map, address):
    self.heap = heap
    self.map = map
    self.address = address

  def Is(self, cls):
    return isinstance(self, cls)

  def Print(self, p):
    p.Print(str(self))

  def __str__(self):
    return "HeapObject(%08x, %s)" % (self.address,
                                     INSTANCE_TYPES[self.map.instance_type])

  def ObjectField(self, offset):
    field_value = self.heap.reader.ReadU32(self.address + offset)
    return self.heap.FindObjectOrSmi(field_value)

  def SmiField(self, offset):
    field_value = self.heap.reader.ReadU32(self.address + offset)
    assert (field_value & 1) == 0
    return field_value / 2


class Map(HeapObject):
  INSTANCE_TYPE_OFFSET = 8

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.instance_type = \
        heap.reader.ReadU8(self.address + Map.INSTANCE_TYPE_OFFSET)


class String(HeapObject):
  LENGTH_OFFSET = 4

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.length = self.SmiField(String.LENGTH_OFFSET)

  def GetChars(self):
    return "?string?"

  def Print(self, p):
    p.Print(str(self))

  def __str__(self):
    return "\"%s\"" % self.GetChars()


class SeqString(String):
  CHARS_OFFSET = 12

  def __init__(self, heap, map, address):
    String.__init__(self, heap, map, address)
    self.chars = heap.reader.ReadBytes(self.address + SeqString.CHARS_OFFSET,
                                       self.length)

  def GetChars(self):
    return self.chars


class ExternalString(String):
  RESOURCE_OFFSET = 12

  WEBKIT_RESOUCE_STRING_IMPL_OFFSET = 4
  WEBKIT_STRING_IMPL_CHARS_OFFSET = 8

  def __init__(self, heap, map, address):
    String.__init__(self, heap, map, address)
    reader = heap.reader
    self.resource = \
        reader.ReadU32(self.address + ExternalString.RESOURCE_OFFSET)
    self.chars = "?external string?"
    if not reader.IsValidAddress(self.resource): return
    string_impl_address = self.resource + \
        ExternalString.WEBKIT_RESOUCE_STRING_IMPL_OFFSET
    if not reader.IsValidAddress(string_impl_address): return
    string_impl = reader.ReadU32(string_impl_address)
    chars_ptr_address = string_impl + \
        ExternalString.WEBKIT_STRING_IMPL_CHARS_OFFSET
    if not reader.IsValidAddress(chars_ptr_address): return
    chars_ptr = reader.ReadU32(chars_ptr_address)
    if not reader.IsValidAddress(chars_ptr): return
    raw_chars = reader.ReadBytes(chars_ptr, 2 * self.length)
    self.chars = codecs.getdecoder("utf16")(raw_chars)[0]

  def GetChars(self):
    return self.chars


class ConsString(String):
  LEFT_OFFSET = 12
  RIGHT_OFFSET = 16

  def __init__(self, heap, map, address):
    String.__init__(self, heap, map, address)
    self.left = self.ObjectField(ConsString.LEFT_OFFSET)
    self.right = self.ObjectField(ConsString.RIGHT_OFFSET)

  def GetChars(self):
    return self.left.GetChars() + self.right.GetChars()


class Oddball(HeapObject):
  TO_STRING_OFFSET = 4

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.to_string = self.ObjectField(Oddball.TO_STRING_OFFSET)

  def Print(self, p):
    p.Print(str(self))

  def __str__(self):
    return "<%s>" % self.to_string.GetChars()


class FixedArray(HeapObject):
  LENGTH_OFFSET = 4
  ELEMENTS_OFFSET = 8

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.length = self.SmiField(FixedArray.LENGTH_OFFSET)

  def Print(self, p):
    p.Print("FixedArray(%08x) {" % self.address)
    p.Indent()
    p.Print("length: %d" % self.length)
    for i in xrange(self.length):
      offset = FixedArray.ELEMENTS_OFFSET + 4 * i
      p.Print("[%08d] = %s" % (i, self.ObjectField(offset)))
    p.Dedent()
    p.Print("}")

  def __str__(self):
    return "FixedArray(%08x, length=%d)" % (self.address, self.length)


class JSFunction(HeapObject):
  CODE_ENTRY_OFFSET = 12
  SHARED_OFFSET = 20

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    code_entry = \
        heap.reader.ReadU32(self.address + JSFunction.CODE_ENTRY_OFFSET)
    self.code = heap.FindObject(code_entry - Code.ENTRY_OFFSET + 1)
    self.shared = self.ObjectField(JSFunction.SHARED_OFFSET)

  def Print(self, p):
    source = "\n".join("  %s" % line for line in self._GetSource().split("\n"))
    p.Print("JSFunction(%08x) {" % self.address)
    p.Indent()
    p.Print("inferred name: %s" % self.shared.inferred_name)
    if self.shared.script.Is(Script) and self.shared.script.name.Is(String):
      p.Print("script name: %s" % self.shared.script.name)
    p.Print("source:")
    p.PrintLines(self._GetSource().split("\n"))
    p.Print("code:")
    self.code.Print(p)
    if self.code != self.shared.code:
      p.Print("unoptimized code:")
      self.shared.code.Print(p)
    p.Dedent()
    p.Print("}")

  def __str__(self):
    inferred_name = ""
    if self.shared.Is(SharedFunctionInfo):
      inferred_name = self.shared.inferred_name
    return "JSFunction(%08x, %s)" % (self.address, inferred_name)

  def _GetSource(self):
    source = "?source?"
    start = self.shared.start_position
    end = self.shared.end_position
    if not self.shared.script.Is(Script): return source
    script_source = self.shared.script.source
    if not script_source.Is(String): return source
    return script_source.GetChars()[start:end]


class SharedFunctionInfo(HeapObject):
  CODE_OFFSET = 2 * 4
  SCRIPT_OFFSET = 7 * 4
  INFERRED_NAME_OFFSET = 9 * 4
  START_POSITION_AND_TYPE_OFFSET = 17 * 4
  END_POSITION_OFFSET = 18 * 4

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.code = self.ObjectField(SharedFunctionInfo.CODE_OFFSET)
    self.script = self.ObjectField(SharedFunctionInfo.SCRIPT_OFFSET)
    self.inferred_name = \
        self.ObjectField(SharedFunctionInfo.INFERRED_NAME_OFFSET)
    start_position_and_type = \
        self.SmiField(SharedFunctionInfo.START_POSITION_AND_TYPE_OFFSET)
    self.start_position = start_position_and_type >> 2
    self.end_position = self.SmiField(SharedFunctionInfo.END_POSITION_OFFSET)


class Script(HeapObject):
  SOURCE_OFFSET = 4
  NAME_OFFSET = 8

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.source = self.ObjectField(Script.SOURCE_OFFSET)
    self.name = self.ObjectField(Script.NAME_OFFSET)


class Code(HeapObject):
  INSTRUCTION_SIZE_OFFSET = 4
  ENTRY_OFFSET = 32

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.entry = self.address + Code.ENTRY_OFFSET
    self.instruction_size = \
        heap.reader.ReadU32(self.address + Code.INSTRUCTION_SIZE_OFFSET)

  def Print(self, p):
    lines = self.heap.reader.GetDisasmLines(self.entry, self.instruction_size)
    p.Print("Code(%08x) {" % self.address)
    p.Indent()
    p.Print("instruction_size: %d" % self.instruction_size)
    p.PrintLines(self._FormatLine(line) for line in lines)
    p.Dedent()
    p.Print("}")

  def _FormatLine(self, line):
    return FormatDisasmLine(self.entry, self.heap, line)


class V8Heap(object):
  CLASS_MAP = {
    "SYMBOL_TYPE": SeqString,
    "ASCII_SYMBOL_TYPE": SeqString,
    "CONS_SYMBOL_TYPE": ConsString,
    "CONS_ASCII_SYMBOL_TYPE": ConsString,
    "EXTERNAL_SYMBOL_TYPE": ExternalString,
    "EXTERNAL_SYMBOL_WITH_ASCII_DATA_TYPE": ExternalString,
    "EXTERNAL_ASCII_SYMBOL_TYPE": ExternalString,
    "STRING_TYPE": SeqString,
    "ASCII_STRING_TYPE": SeqString,
    "CONS_STRING_TYPE": ConsString,
    "CONS_ASCII_STRING_TYPE": ConsString,
    "EXTERNAL_STRING_TYPE": ExternalString,
    "EXTERNAL_STRING_WITH_ASCII_DATA_TYPE": ExternalString,
    "EXTERNAL_ASCII_STRING_TYPE": ExternalString,

    "MAP_TYPE": Map,
    "ODDBALL_TYPE": Oddball,
    "FIXED_ARRAY_TYPE": FixedArray,
    "JS_FUNCTION_TYPE": JSFunction,
    "SHARED_FUNCTION_INFO_TYPE": SharedFunctionInfo,
    "SCRIPT_TYPE": Script,
    "CODE_TYPE": Code
  }

  def __init__(self, reader, stack_map):
    self.reader = reader
    self.stack_map = stack_map
    self.objects = {}

  def FindObjectOrSmi(self, tagged_address):
    if (tagged_address & 1) == 0: return tagged_address / 2
    return self.FindObject(tagged_address)

  def FindObject(self, tagged_address):
    if tagged_address in self.objects:
      return self.objects[tagged_address]
    if (tagged_address & 1) != 1: return None
    address = tagged_address - 1
    if not self.reader.IsValidAddress(address): return None
    map_tagged_address = self.reader.ReadU32(address)
    if tagged_address == map_tagged_address:
      # Meta map?
      meta_map = Map(self, None, address)
      instance_type_name = INSTANCE_TYPES.get(meta_map.instance_type)
      if instance_type_name != "MAP_TYPE": return None
      meta_map.map = meta_map
      object = meta_map
    else:
      map = self.FindObject(map_tagged_address)
      if map is None: return None
      instance_type_name = INSTANCE_TYPES.get(map.instance_type)
      if instance_type_name is None: return None
      cls = V8Heap.CLASS_MAP.get(instance_type_name, HeapObject)
      object = cls(self, map, address)
    self.objects[tagged_address] = object
    return object


EIP_PROXIMITY = 64


def AnalyzeMinidump(options, minidump_name):
  reader = MinidumpReader(options, minidump_name)
  DebugPrint("========================================")
  if reader.exception is None:
    print "Minidump has no exception info"
    return
  print "Exception info:"
  exception_thread = reader.thread_map[reader.exception.thread_id]
  print "  thread id: %d" % exception_thread.id
  print "  code: %08X" % reader.exception.exception.code
  print "  context:"
  print "    eax: %08x" % reader.exception_context.eax
  print "    ebx: %08x" % reader.exception_context.ebx
  print "    ecx: %08x" % reader.exception_context.ecx
  print "    edx: %08x" % reader.exception_context.edx
  print "    edi: %08x" % reader.exception_context.edi
  print "    esi: %08x" % reader.exception_context.esi
  print "    ebp: %08x" % reader.exception_context.ebp
  print "    esp: %08x" % reader.exception_context.esp
  print "    eip: %08x" % reader.exception_context.eip
  # TODO(vitalyr): decode eflags.
  print "    eflags: %s" % bin(reader.exception_context.eflags)[2:]
  print

  stack_bottom = exception_thread.stack.start + \
      exception_thread.stack.memory.data_size
  stack_map = {reader.exception_context.eip: -1}
  for slot in xrange(reader.exception_context.esp, stack_bottom, 4):
    maybe_address = reader.ReadU32(slot)
    if not maybe_address in stack_map:
      stack_map[maybe_address] = slot
  heap = V8Heap(reader, stack_map)

  print "Disassembly around exception.eip:"
  start = reader.exception_context.eip - EIP_PROXIMITY
  lines = reader.GetDisasmLines(start, 2 * EIP_PROXIMITY)
  for line in lines:
    print FormatDisasmLine(start, heap, line)
  print

  print "Annotated stack (from exception.esp to bottom):"
  for slot in xrange(reader.exception_context.esp, stack_bottom, 4):
    maybe_address = reader.ReadU32(slot)
    heap_object = heap.FindObject(maybe_address)
    print "%08x: %08x" % (slot, maybe_address)
    if heap_object:
      heap_object.Print(Printer())
      print

  reader.Dispose()


if __name__ == "__main__":
  parser = optparse.OptionParser(USAGE)
  options, args = parser.parse_args()
  if len(args) != 1:
    parser.print_help()
    sys.exit(1)
  AnalyzeMinidump(options, args[0])
