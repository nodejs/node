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

MD_CONTEXT_AMD64 = 0x00100000
MD_CONTEXT_AMD64_CONTROL = (MD_CONTEXT_AMD64 | 0x00000001)
MD_CONTEXT_AMD64_INTEGER = (MD_CONTEXT_AMD64 | 0x00000002)
MD_CONTEXT_AMD64_SEGMENTS = (MD_CONTEXT_AMD64 | 0x00000004)
MD_CONTEXT_AMD64_FLOATING_POINT = (MD_CONTEXT_AMD64 | 0x00000008)
MD_CONTEXT_AMD64_DEBUG_REGISTERS = (MD_CONTEXT_AMD64 | 0x00000010)

MINIDUMP_CONTEXT_AMD64 = Descriptor([
  ("p1_home", ctypes.c_uint64),
  ("p2_home", ctypes.c_uint64),
  ("p3_home", ctypes.c_uint64),
  ("p4_home", ctypes.c_uint64),
  ("p5_home", ctypes.c_uint64),
  ("p6_home", ctypes.c_uint64),
  ("context_flags", ctypes.c_uint32),
  ("mx_csr", ctypes.c_uint32),
  # MD_CONTEXT_AMD64_CONTROL.
  ("cs", EnableOnFlag(ctypes.c_uint16, MD_CONTEXT_AMD64_CONTROL)),
  # MD_CONTEXT_AMD64_SEGMENTS
  ("ds", EnableOnFlag(ctypes.c_uint16, MD_CONTEXT_AMD64_SEGMENTS)),
  ("es", EnableOnFlag(ctypes.c_uint16, MD_CONTEXT_AMD64_SEGMENTS)),
  ("fs", EnableOnFlag(ctypes.c_uint16, MD_CONTEXT_AMD64_SEGMENTS)),
  ("gs", EnableOnFlag(ctypes.c_uint16, MD_CONTEXT_AMD64_SEGMENTS)),
  # MD_CONTEXT_AMD64_CONTROL.
  ("ss", EnableOnFlag(ctypes.c_uint16, MD_CONTEXT_AMD64_CONTROL)),
  ("eflags", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_AMD64_CONTROL)),
  # MD_CONTEXT_AMD64_DEBUG_REGISTERS.
  ("dr0", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("dr1", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("dr2", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("dr3", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("dr6", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("dr7", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  # MD_CONTEXT_AMD64_INTEGER.
  ("rax", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("rcx", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("rdx", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("rbx", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  # MD_CONTEXT_AMD64_CONTROL.
  ("rsp", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_CONTROL)),
  # MD_CONTEXT_AMD64_INTEGER.
  ("rbp", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("rsi", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("rdi", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("r8", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("r9", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("r10", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("r11", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("r12", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("r13", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("r14", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  ("r15", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_INTEGER)),
  # MD_CONTEXT_AMD64_CONTROL.
  ("rip", EnableOnFlag(ctypes.c_uint64, MD_CONTEXT_AMD64_CONTROL)),
  # MD_CONTEXT_AMD64_FLOATING_POINT
  ("sse_registers", EnableOnFlag(ctypes.c_uint8 * (16 * 26),
                                 MD_CONTEXT_AMD64_FLOATING_POINT)),
  ("vector_registers", EnableOnFlag(ctypes.c_uint8 * (16 * 26),
                                    MD_CONTEXT_AMD64_FLOATING_POINT)),
  ("vector_control", EnableOnFlag(ctypes.c_uint64,
                                  MD_CONTEXT_AMD64_FLOATING_POINT)),
  # MD_CONTEXT_AMD64_DEBUG_REGISTERS.
  ("debug_control", EnableOnFlag(ctypes.c_uint64,
                                 MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("last_branch_to_rip", EnableOnFlag(ctypes.c_uint64,
                                      MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("last_branch_from_rip", EnableOnFlag(ctypes.c_uint64,
                                        MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("last_exception_to_rip", EnableOnFlag(ctypes.c_uint64,
                                         MD_CONTEXT_AMD64_DEBUG_REGISTERS)),
  ("last_exception_from_rip", EnableOnFlag(ctypes.c_uint64,
                                           MD_CONTEXT_AMD64_DEBUG_REGISTERS))
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

MINIDUMP_RAW_SYSTEM_INFO = Descriptor([
  ("processor_architecture", ctypes.c_uint16)
])

MD_CPU_ARCHITECTURE_X86 = 0
MD_CPU_ARCHITECTURE_AMD64 = 9

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
    self.arch = None
    self.exception = None
    self.exception_context = None
    self.memory_list = None
    self.memory_list64 = None
    self.thread_map = {}

    # Find MDRawSystemInfo stream and determine arch.
    for d in directories:
      if d.stream_type == MD_SYSTEM_INFO_STREAM:
        system_info = MINIDUMP_RAW_SYSTEM_INFO.Read(
            self.minidump, d.location.rva)
        self.arch = system_info.processor_architecture
        assert self.arch in [MD_CPU_ARCHITECTURE_AMD64, MD_CPU_ARCHITECTURE_X86]
    assert not self.arch is None

    for d in directories:
      DebugPrint(d)
      if d.stream_type == MD_EXCEPTION_STREAM:
        self.exception = MINIDUMP_EXCEPTION_STREAM.Read(
          self.minidump, d.location.rva)
        DebugPrint(self.exception)
        if self.arch == MD_CPU_ARCHITECTURE_X86:
          self.exception_context = MINIDUMP_CONTEXT_X86.Read(
              self.minidump, self.exception.thread_context.rva)
        elif self.arch == MD_CPU_ARCHITECTURE_AMD64:
          self.exception_context = MINIDUMP_CONTEXT_AMD64.Read(
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
        assert self.memory_list is None
        self.memory_list = MINIDUMP_MEMORY_LIST.Read(
          self.minidump, d.location.rva)
        assert ctypes.sizeof(self.memory_list) == d.location.data_size
        DebugPrint(self.memory_list)
      elif d.stream_type == MD_MEMORY_64_LIST_STREAM:
        assert self.memory_list64 is None
        self.memory_list64 = MINIDUMP_MEMORY_LIST64.Read(
          self.minidump, d.location.rva)
        assert ctypes.sizeof(self.memory_list64) == d.location.data_size
        DebugPrint(self.memory_list64)

  def IsValidAddress(self, address):
    return self.FindLocation(address) is not None

  def ReadU8(self, address):
    location = self.FindLocation(address)
    return ctypes.c_uint8.from_buffer(self.minidump, location).value

  def ReadU32(self, address):
    location = self.FindLocation(address)
    return ctypes.c_uint32.from_buffer(self.minidump, location).value

  def ReadU64(self, address):
    location = self.FindLocation(address)
    return ctypes.c_uint64.from_buffer(self.minidump, location).value

  def ReadUIntPtr(self, address):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return self.ReadU64(address)
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return self.ReadU32(address)

  def ReadBytes(self, address, size):
    location = self.FindLocation(address)
    return self.minidump[location:location + size]

  def FindLocation(self, address):
    offset = 0
    if self.memory_list64 is not None:
      for r in self.memory_list64.ranges:
        if r.start <= address < r.start + r.size:
          return self.memory_list64.base_rva + offset + address - r.start
        offset += r.size
    if self.memory_list is not None:
      for r in self.memory_list.ranges:
        if r.start <= address < r.start + r.memory.data_size:
          return r.memory.rva + address - r.start
    return None

  def GetDisasmLines(self, address, size):
    location = self.FindLocation(address)
    if location is None: return []
    arch = None
    if self.arch == MD_CPU_ARCHITECTURE_X86:
      arch = "ia32"
    elif self.arch == MD_CPU_ARCHITECTURE_AMD64:
      arch = "x64"
    return disasm.GetDisasmLines(self.minidump_name,
                                 location,
                                 size,
                                 arch,
                                 False)


  def Dispose(self):
    self.minidump.close()
    self.minidump_file.close()

  def ExceptionIP(self):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return self.exception_context.rip
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return self.exception_context.eip

  def ExceptionSP(self):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return self.exception_context.rsp
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return self.exception_context.esp

  def FormatIntPtr(self, value):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return "%016x" % value
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return "%08x" % value

  def PointerSize(self):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return 8
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return 4

  def Register(self, name):
    return self.exception_context.__getattribute__(name)


# List of V8 instance types. Obtained by adding the code below to any .cc file.
#
# #define DUMP_TYPE(T) printf("  %d: \"%s\",\n", T, #T);
# struct P {
#   P() {
#     printf("INSTANCE_TYPES = {\n");
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
  82: "SHORT_EXTERNAL_SYMBOL_TYPE",
  90: "SHORT_EXTERNAL_SYMBOL_WITH_ASCII_DATA_TYPE",
  86: "SHORT_EXTERNAL_ASCII_SYMBOL_TYPE",
  0: "STRING_TYPE",
  4: "ASCII_STRING_TYPE",
  1: "CONS_STRING_TYPE",
  5: "CONS_ASCII_STRING_TYPE",
  3: "SLICED_STRING_TYPE",
  2: "EXTERNAL_STRING_TYPE",
  10: "EXTERNAL_STRING_WITH_ASCII_DATA_TYPE",
  6: "EXTERNAL_ASCII_STRING_TYPE",
  18: "SHORT_EXTERNAL_STRING_TYPE",
  26: "SHORT_EXTERNAL_STRING_WITH_ASCII_DATA_TYPE",
  22: "SHORT_EXTERNAL_ASCII_STRING_TYPE",
  6: "PRIVATE_EXTERNAL_ASCII_STRING_TYPE",
  128: "MAP_TYPE",
  129: "CODE_TYPE",
  130: "ODDBALL_TYPE",
  131: "JS_GLOBAL_PROPERTY_CELL_TYPE",
  132: "HEAP_NUMBER_TYPE",
  133: "FOREIGN_TYPE",
  134: "BYTE_ARRAY_TYPE",
  135: "FREE_SPACE_TYPE",
  136: "EXTERNAL_BYTE_ARRAY_TYPE",
  137: "EXTERNAL_UNSIGNED_BYTE_ARRAY_TYPE",
  138: "EXTERNAL_SHORT_ARRAY_TYPE",
  139: "EXTERNAL_UNSIGNED_SHORT_ARRAY_TYPE",
  140: "EXTERNAL_INT_ARRAY_TYPE",
  141: "EXTERNAL_UNSIGNED_INT_ARRAY_TYPE",
  142: "EXTERNAL_FLOAT_ARRAY_TYPE",
  144: "EXTERNAL_PIXEL_ARRAY_TYPE",
  146: "FILLER_TYPE",
  147: "ACCESSOR_INFO_TYPE",
  148: "ACCESSOR_PAIR_TYPE",
  149: "ACCESS_CHECK_INFO_TYPE",
  150: "INTERCEPTOR_INFO_TYPE",
  151: "CALL_HANDLER_INFO_TYPE",
  152: "FUNCTION_TEMPLATE_INFO_TYPE",
  153: "OBJECT_TEMPLATE_INFO_TYPE",
  154: "SIGNATURE_INFO_TYPE",
  155: "TYPE_SWITCH_INFO_TYPE",
  156: "SCRIPT_TYPE",
  157: "CODE_CACHE_TYPE",
  158: "POLYMORPHIC_CODE_CACHE_TYPE",
  161: "FIXED_ARRAY_TYPE",
  145: "FIXED_DOUBLE_ARRAY_TYPE",
  162: "SHARED_FUNCTION_INFO_TYPE",
  163: "JS_MESSAGE_OBJECT_TYPE",
  166: "JS_VALUE_TYPE",
  167: "JS_OBJECT_TYPE",
  168: "JS_CONTEXT_EXTENSION_OBJECT_TYPE",
  169: "JS_GLOBAL_OBJECT_TYPE",
  170: "JS_BUILTINS_OBJECT_TYPE",
  171: "JS_GLOBAL_PROXY_TYPE",
  172: "JS_ARRAY_TYPE",
  165: "JS_PROXY_TYPE",
  175: "JS_WEAK_MAP_TYPE",
  176: "JS_REGEXP_TYPE",
  177: "JS_FUNCTION_TYPE",
  164: "JS_FUNCTION_PROXY_TYPE",
  159: "DEBUG_INFO_TYPE",
  160: "BREAK_POINT_INFO_TYPE",
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
    return "HeapObject(%s, %s)" % (self.heap.reader.FormatIntPtr(self.address),
                                   INSTANCE_TYPES[self.map.instance_type])

  def ObjectField(self, offset):
    field_value = self.heap.reader.ReadUIntPtr(self.address + offset)
    return self.heap.FindObjectOrSmi(field_value)

  def SmiField(self, offset):
    field_value = self.heap.reader.ReadUIntPtr(self.address + offset)
    assert (field_value & 1) == 0
    return field_value / 2


class Map(HeapObject):
  def InstanceTypeOffset(self):
    return self.heap.PointerSize() + self.heap.IntSize()

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.instance_type = \
        heap.reader.ReadU8(self.address + self.InstanceTypeOffset())


class String(HeapObject):
  def LengthOffset(self):
    return self.heap.PointerSize()

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.length = self.SmiField(self.LengthOffset())

  def GetChars(self):
    return "?string?"

  def Print(self, p):
    p.Print(str(self))

  def __str__(self):
    return "\"%s\"" % self.GetChars()


class SeqString(String):
  def CharsOffset(self):
    return self.heap.PointerSize() * 3

  def __init__(self, heap, map, address):
    String.__init__(self, heap, map, address)
    self.chars = heap.reader.ReadBytes(self.address + self.CharsOffset(),
                                       self.length)

  def GetChars(self):
    return self.chars


class ExternalString(String):
  # TODO(vegorov) fix ExternalString for X64 architecture
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
  def LeftOffset(self):
    return self.heap.PointerSize() * 3

  def RightOffset(self):
    return self.heap.PointerSize() * 4

  def __init__(self, heap, map, address):
    String.__init__(self, heap, map, address)
    self.left = self.ObjectField(self.LeftOffset())
    self.right = self.ObjectField(self.RightOffset())

  def GetChars(self):
    return self.left.GetChars() + self.right.GetChars()


class Oddball(HeapObject):
  def ToStringOffset(self):
    return self.heap.PointerSize()

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.to_string = self.ObjectField(self.ToStringOffset())

  def Print(self, p):
    p.Print(str(self))

  def __str__(self):
    return "<%s>" % self.to_string.GetChars()


class FixedArray(HeapObject):
  def LengthOffset(self):
    return self.heap.PointerSize()

  def ElementsOffset(self):
    return self.heap.PointerSize() * 2

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.length = self.SmiField(self.LengthOffset())

  def Print(self, p):
    p.Print("FixedArray(%s) {" % self.heap.reader.FormatIntPtr(self.address))
    p.Indent()
    p.Print("length: %d" % self.length)
    base_offset = self.ElementsOffset()
    for i in xrange(self.length):
      offset = base_offset + 4 * i
      p.Print("[%08d] = %s" % (i, self.ObjectField(offset)))
    p.Dedent()
    p.Print("}")

  def __str__(self):
    return "FixedArray(%08x, length=%d)" % (self.address, self.length)


class JSFunction(HeapObject):
  def CodeEntryOffset(self):
    return 3 * self.heap.PointerSize()

  def SharedOffset(self):
    return 5 * self.heap.PointerSize()

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    code_entry = \
        heap.reader.ReadU32(self.address + self.CodeEntryOffset())
    self.code = heap.FindObject(code_entry - Code.HeaderSize(heap) + 1)
    self.shared = self.ObjectField(self.SharedOffset())

  def Print(self, p):
    source = "\n".join("  %s" % line for line in self._GetSource().split("\n"))
    p.Print("JSFunction(%s) {" % self.heap.reader.FormatIntPtr(self.address))
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
    return "JSFunction(%s, %s)" % \
          (self.heap.reader.FormatIntPtr(self.address), inferred_name)

  def _GetSource(self):
    source = "?source?"
    start = self.shared.start_position
    end = self.shared.end_position
    if not self.shared.script.Is(Script): return source
    script_source = self.shared.script.source
    if not script_source.Is(String): return source
    return script_source.GetChars()[start:end]


class SharedFunctionInfo(HeapObject):
  def CodeOffset(self):
    return 2 * self.heap.PointerSize()

  def ScriptOffset(self):
    return 7 * self.heap.PointerSize()

  def InferredNameOffset(self):
    return 9 * self.heap.PointerSize()

  def EndPositionOffset(self):
    return 12 * self.heap.PointerSize() + 4 * self.heap.IntSize()

  def StartPositionAndTypeOffset(self):
    return 12 * self.heap.PointerSize() + 5 * self.heap.IntSize()

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.code = self.ObjectField(self.CodeOffset())
    self.script = self.ObjectField(self.ScriptOffset())
    self.inferred_name = self.ObjectField(self.InferredNameOffset())
    if heap.PointerSize() == 8:
      start_position_and_type = \
          heap.reader.ReadU32(self.StartPositionAndTypeOffset())
      self.start_position = start_position_and_type >> 2
      pseudo_smi_end_position = \
          heap.reader.ReadU32(self.EndPositionOffset())
      self.end_position = pseudo_smi_end_position >> 2
    else:
      start_position_and_type = \
          self.SmiField(self.StartPositionAndTypeOffset())
      self.start_position = start_position_and_type >> 2
      self.end_position = \
          self.SmiField(self.EndPositionOffset())


class Script(HeapObject):
  def SourceOffset(self):
    return self.heap.PointerSize()

  def NameOffset(self):
    return self.SourceOffset() + self.heap.PointerSize()

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.source = self.ObjectField(self.SourceOffset())
    self.name = self.ObjectField(self.NameOffset())


class Code(HeapObject):
  CODE_ALIGNMENT_MASK = (1 << 5) - 1

  def InstructionSizeOffset(self):
    return self.heap.PointerSize()

  @staticmethod
  def HeaderSize(heap):
    return (heap.PointerSize() + heap.IntSize() + \
        4 * heap.PointerSize() + 3 * heap.IntSize() + \
        Code.CODE_ALIGNMENT_MASK) & ~Code.CODE_ALIGNMENT_MASK

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.entry = self.address + Code.HeaderSize(heap)
    self.instruction_size = \
        heap.reader.ReadU32(self.address + self.InstructionSizeOffset())

  def Print(self, p):
    lines = self.heap.reader.GetDisasmLines(self.entry, self.instruction_size)
    p.Print("Code(%s) {" % self.heap.reader.FormatIntPtr(self.address))
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
    "SHORT_EXTERNAL_SYMBOL_TYPE": ExternalString,
    "SHORT_EXTERNAL_SYMBOL_WITH_ASCII_DATA_TYPE": ExternalString,
    "SHORT_EXTERNAL_ASCII_SYMBOL_TYPE": ExternalString,
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
    if (tagged_address & self.ObjectAlignmentMask()) != 1: return None
    address = tagged_address - 1
    if not self.reader.IsValidAddress(address): return None
    map_tagged_address = self.reader.ReadUIntPtr(address)
    if tagged_address == map_tagged_address:
      # Meta map?
      meta_map = Map(self, None, address)
      instance_type_name = INSTANCE_TYPES.get(meta_map.instance_type)
      if instance_type_name != "MAP_TYPE": return None
      meta_map.map = meta_map
      object = meta_map
    else:
      map = self.FindMap(map_tagged_address)
      if map is None: return None
      instance_type_name = INSTANCE_TYPES.get(map.instance_type)
      if instance_type_name is None: return None
      cls = V8Heap.CLASS_MAP.get(instance_type_name, HeapObject)
      object = cls(self, map, address)
    self.objects[tagged_address] = object
    return object

  def FindMap(self, tagged_address):
    if (tagged_address & self.MapAlignmentMask()) != 1: return None
    address = tagged_address - 1
    if not self.reader.IsValidAddress(address): return None
    object = Map(self, None, address)
    return object

  def IntSize(self):
    return 4

  def PointerSize(self):
    return self.reader.PointerSize()

  def ObjectAlignmentMask(self):
    return self.PointerSize() - 1

  def MapAlignmentMask(self):
    if self.reader.arch == MD_CPU_ARCHITECTURE_AMD64:
      return (1 << 4) - 1
    elif self.reader.arch == MD_CPU_ARCHITECTURE_X86:
      return (1 << 5) - 1


EIP_PROXIMITY = 64

CONTEXT_FOR_ARCH = {
    MD_CPU_ARCHITECTURE_AMD64:
      ['rax', 'rbx', 'rcx', 'rdx', 'rdi', 'rsi', 'rbp', 'rsp', 'rip'],
    MD_CPU_ARCHITECTURE_X86:
      ['eax', 'ebx', 'ecx', 'edx', 'edi', 'esi', 'ebp', 'esp', 'eip']
}

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
  for r in CONTEXT_FOR_ARCH[reader.arch]:
    print "    %s: %s" % (r, reader.FormatIntPtr(reader.Register(r)))
  # TODO(vitalyr): decode eflags.
  print "    eflags: %s" % bin(reader.exception_context.eflags)[2:]
  print

  stack_top = reader.ExceptionSP()
  stack_bottom = exception_thread.stack.start + \
      exception_thread.stack.memory.data_size
  stack_map = {reader.ExceptionIP(): -1}
  for slot in xrange(stack_top, stack_bottom, reader.PointerSize()):
    maybe_address = reader.ReadUIntPtr(slot)
    if not maybe_address in stack_map:
      stack_map[maybe_address] = slot
  heap = V8Heap(reader, stack_map)

  print "Disassembly around exception.eip:"
  start = reader.ExceptionIP() - EIP_PROXIMITY
  lines = reader.GetDisasmLines(start, 2 * EIP_PROXIMITY)
  for line in lines:
    print FormatDisasmLine(start, heap, line)
  print

  print "Annotated stack (from exception.esp to bottom):"
  for slot in xrange(stack_top, stack_bottom, reader.PointerSize()):
    maybe_address = reader.ReadUIntPtr(slot)
    heap_object = heap.FindObject(maybe_address)
    print "%s: %s" % (reader.FormatIntPtr(slot),
                      reader.FormatIntPtr(maybe_address))
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
