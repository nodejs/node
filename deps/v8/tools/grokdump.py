#!/usr/bin/env python
#
# Copyright 2012 the V8 project authors. All rights reserved.
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

import bisect
import cmd
import codecs
import ctypes
import datetime
import disasm
import mmap
import optparse
import os
import re
import struct
import sys
import types
import v8heapconst

USAGE="""usage: %prog [OPTIONS] [DUMP-FILE]

Minidump analyzer.

Shows the processor state at the point of exception including the
stack of the active thread and the referenced objects in the V8
heap. Code objects are disassembled and the addresses linked from the
stack (e.g. pushed return addresses) are marked with "=>".

Examples:
  $ %prog 12345678-1234-1234-1234-123456789abcd-full.dmp"""


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


def FullDump(reader, heap):
  """Dump all available memory regions."""
  def dump_region(reader, start, size, location):
    print
    while start & 3 != 0:
      start += 1
      size -= 1
      location += 1
    is_executable = reader.IsProbableExecutableRegion(location, size)
    is_ascii = reader.IsProbableASCIIRegion(location, size)

    if is_executable is not False:
      lines = reader.GetDisasmLines(start, size)
      for line in lines:
        print FormatDisasmLine(start, heap, line)
      print

    if is_ascii is not False:
      # Output in the same format as the Unix hd command
      addr = start
      for slot in xrange(location, location + size, 16):
        hex_line = ""
        asc_line = ""
        for i in xrange(0, 16):
          if slot + i < location + size:
            byte = ctypes.c_uint8.from_buffer(reader.minidump, slot + i).value
            if byte >= 0x20 and byte < 0x7f:
              asc_line += chr(byte)
            else:
              asc_line += "."
            hex_line += " %02x" % (byte)
          else:
            hex_line += "   "
          if i == 7:
            hex_line += " "
        print "%s  %s |%s|" % (reader.FormatIntPtr(addr),
                               hex_line,
                               asc_line)
        addr += 16

    if is_executable is not True and is_ascii is not True:
      print "%s - %s" % (reader.FormatIntPtr(start),
                         reader.FormatIntPtr(start + size))
      for slot in xrange(start,
                         start + size,
                         reader.PointerSize()):
        maybe_address = reader.ReadUIntPtr(slot)
        heap_object = heap.FindObject(maybe_address)
        print "%s: %s" % (reader.FormatIntPtr(slot),
                          reader.FormatIntPtr(maybe_address))
        if heap_object:
          heap_object.Print(Printer())
          print

  reader.ForEachMemoryRegion(dump_region)

# Heap constants generated by 'make grokdump' in v8heapconst module.
INSTANCE_TYPES = v8heapconst.INSTANCE_TYPES
KNOWN_MAPS = v8heapconst.KNOWN_MAPS
KNOWN_OBJECTS = v8heapconst.KNOWN_OBJECTS

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

MINIDUMP_STRING = Descriptor([
  ("length", ctypes.c_uint32),
  ("buffer", lambda t: ctypes.c_uint8 * (t.length + 2))
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

MD_CONTEXT_ARM = 0x40000000
MD_CONTEXT_ARM_INTEGER = (MD_CONTEXT_ARM | 0x00000002)
MD_CONTEXT_ARM_FLOATING_POINT = (MD_CONTEXT_ARM | 0x00000004)
MD_FLOATINGSAVEAREA_ARM_FPR_COUNT = 32
MD_FLOATINGSAVEAREA_ARM_FPEXTRA_COUNT = 8

MINIDUMP_FLOATING_SAVE_AREA_ARM = Descriptor([
  ("fpscr", ctypes.c_uint64),
  ("regs", ctypes.c_uint64 * MD_FLOATINGSAVEAREA_ARM_FPR_COUNT),
  ("extra", ctypes.c_uint64 * MD_FLOATINGSAVEAREA_ARM_FPEXTRA_COUNT)
])

MINIDUMP_CONTEXT_ARM = Descriptor([
  ("context_flags", ctypes.c_uint32),
  # MD_CONTEXT_ARM_INTEGER.
  ("r0", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r1", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r2", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r3", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r4", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r5", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r6", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r7", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r8", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r9", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r10", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r11", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("r12", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("sp", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("lr", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("pc", EnableOnFlag(ctypes.c_uint32, MD_CONTEXT_ARM_INTEGER)),
  ("cpsr", ctypes.c_uint32),
  ("float_save", EnableOnFlag(MINIDUMP_FLOATING_SAVE_AREA_ARM.ctype,
                              MD_CONTEXT_ARM_FLOATING_POINT))
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

MINIDUMP_VS_FIXEDFILEINFO = Descriptor([
  ("dwSignature", ctypes.c_uint32),
  ("dwStrucVersion", ctypes.c_uint32),
  ("dwFileVersionMS", ctypes.c_uint32),
  ("dwFileVersionLS", ctypes.c_uint32),
  ("dwProductVersionMS", ctypes.c_uint32),
  ("dwProductVersionLS", ctypes.c_uint32),
  ("dwFileFlagsMask", ctypes.c_uint32),
  ("dwFileFlags", ctypes.c_uint32),
  ("dwFileOS", ctypes.c_uint32),
  ("dwFileType", ctypes.c_uint32),
  ("dwFileSubtype", ctypes.c_uint32),
  ("dwFileDateMS", ctypes.c_uint32),
  ("dwFileDateLS", ctypes.c_uint32)
])

MINIDUMP_RAW_MODULE = Descriptor([
  ("base_of_image", ctypes.c_uint64),
  ("size_of_image", ctypes.c_uint32),
  ("checksum", ctypes.c_uint32),
  ("time_date_stamp", ctypes.c_uint32),
  ("module_name_rva", ctypes.c_uint32),
  ("version_info", MINIDUMP_VS_FIXEDFILEINFO.ctype),
  ("cv_record", MINIDUMP_LOCATION_DESCRIPTOR.ctype),
  ("misc_record", MINIDUMP_LOCATION_DESCRIPTOR.ctype),
  ("reserved0", ctypes.c_uint32 * 2),
  ("reserved1", ctypes.c_uint32 * 2)
])

MINIDUMP_MODULE_LIST = Descriptor([
  ("number_of_modules", ctypes.c_uint32),
  ("modules", lambda t: MINIDUMP_RAW_MODULE.ctype * t.number_of_modules)
])

MINIDUMP_RAW_SYSTEM_INFO = Descriptor([
  ("processor_architecture", ctypes.c_uint16)
])

MD_CPU_ARCHITECTURE_X86 = 0
MD_CPU_ARCHITECTURE_ARM = 5
MD_CPU_ARCHITECTURE_AMD64 = 9

class FuncSymbol:
  def __init__(self, start, size, name):
    self.start = start
    self.end = self.start + size
    self.name = name

  def __cmp__(self, other):
    if isinstance(other, FuncSymbol):
      return self.start - other.start
    return self.start - other

  def Covers(self, addr):
    return (self.start <= addr) and (addr < self.end)

class MinidumpReader(object):
  """Minidump (.dmp) reader."""

  _HEADER_MAGIC = 0x504d444d

  def __init__(self, options, minidump_name):
    self.minidump_name = minidump_name
    self.minidump_file = open(minidump_name, "r")
    self.minidump = mmap.mmap(self.minidump_file.fileno(), 0, mmap.MAP_PRIVATE)
    self.header = MINIDUMP_HEADER.Read(self.minidump, 0)
    if self.header.signature != MinidumpReader._HEADER_MAGIC:
      print >>sys.stderr, "Warning: Unsupported minidump header magic!"
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
    self.module_list = None
    self.thread_map = {}

    self.symdir = options.symdir
    self.modules_with_symbols = []
    self.symbols = []

    # Find MDRawSystemInfo stream and determine arch.
    for d in directories:
      if d.stream_type == MD_SYSTEM_INFO_STREAM:
        system_info = MINIDUMP_RAW_SYSTEM_INFO.Read(
            self.minidump, d.location.rva)
        self.arch = system_info.processor_architecture
        assert self.arch in [MD_CPU_ARCHITECTURE_AMD64,
                             MD_CPU_ARCHITECTURE_ARM,
                             MD_CPU_ARCHITECTURE_X86]
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
        elif self.arch == MD_CPU_ARCHITECTURE_ARM:
          self.exception_context = MINIDUMP_CONTEXT_ARM.Read(
              self.minidump, self.exception.thread_context.rva)
        DebugPrint(self.exception_context)
      elif d.stream_type == MD_THREAD_LIST_STREAM:
        thread_list = MINIDUMP_THREAD_LIST.Read(self.minidump, d.location.rva)
        assert ctypes.sizeof(thread_list) == d.location.data_size
        DebugPrint(thread_list)
        for thread in thread_list.threads:
          DebugPrint(thread)
          self.thread_map[thread.id] = thread
      elif d.stream_type == MD_MODULE_LIST_STREAM:
        assert self.module_list is None
        self.module_list = MINIDUMP_MODULE_LIST.Read(
          self.minidump, d.location.rva)
        assert ctypes.sizeof(self.module_list) == d.location.data_size
      elif d.stream_type == MD_MEMORY_LIST_STREAM:
        print >>sys.stderr, "Warning: This is not a full minidump!"
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
    elif self.arch == MD_CPU_ARCHITECTURE_ARM:
      return self.ReadU32(address)
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return self.ReadU32(address)

  def ReadBytes(self, address, size):
    location = self.FindLocation(address)
    return self.minidump[location:location + size]

  def _ReadWord(self, location):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return ctypes.c_uint64.from_buffer(self.minidump, location).value
    elif self.arch == MD_CPU_ARCHITECTURE_ARM:
      return ctypes.c_uint32.from_buffer(self.minidump, location).value
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return ctypes.c_uint32.from_buffer(self.minidump, location).value

  def IsProbableASCIIRegion(self, location, length):
    ascii_bytes = 0
    non_ascii_bytes = 0
    for loc in xrange(location, location + length):
      byte = ctypes.c_uint8.from_buffer(self.minidump, loc).value
      if byte >= 0x7f:
        non_ascii_bytes += 1
      if byte < 0x20 and byte != 0:
        non_ascii_bytes += 1
      if byte < 0x7f and byte >= 0x20:
        ascii_bytes += 1
      if byte == 0xa:  # newline
        ascii_bytes += 1
    if ascii_bytes * 10 <= length:
      return False
    if length > 0 and ascii_bytes > non_ascii_bytes * 7:
      return True
    if ascii_bytes > non_ascii_bytes * 3:
      return None  # Maybe
    return False

  def IsProbableExecutableRegion(self, location, length):
    opcode_bytes = 0
    sixty_four = self.arch == MD_CPU_ARCHITECTURE_AMD64
    for loc in xrange(location, location + length):
      byte = ctypes.c_uint8.from_buffer(self.minidump, loc).value
      if (byte == 0x8b or           # mov
          byte == 0x89 or           # mov reg-reg
          (byte & 0xf0) == 0x50 or  # push/pop
          (sixty_four and (byte & 0xf0) == 0x40) or  # rex prefix
          byte == 0xc3 or           # return
          byte == 0x74 or           # jeq
          byte == 0x84 or           # jeq far
          byte == 0x75 or           # jne
          byte == 0x85 or           # jne far
          byte == 0xe8 or           # call
          byte == 0xe9 or           # jmp far
          byte == 0xeb):            # jmp near
        opcode_bytes += 1
    opcode_percent = (opcode_bytes * 100) / length
    threshold = 20
    if opcode_percent > threshold + 2:
      return True
    if opcode_percent > threshold - 2:
      return None  # Maybe
    return False

  def FindRegion(self, addr):
    answer = [-1, -1]
    def is_in(reader, start, size, location):
      if addr >= start and addr < start + size:
        answer[0] = start
        answer[1] = size
    self.ForEachMemoryRegion(is_in)
    if answer[0] == -1:
      return None
    return answer

  def ForEachMemoryRegion(self, cb):
    if self.memory_list64 is not None:
      for r in self.memory_list64.ranges:
        location = self.memory_list64.base_rva + offset
        cb(self, r.start, r.size, location)
        offset += r.size

    if self.memory_list is not None:
      for r in self.memory_list.ranges:
        cb(self, r.start, r.memory.data_size, r.memory.rva)

  def FindWord(self, word, alignment=0):
    def search_inside_region(reader, start, size, location):
      location = (location + alignment) & ~alignment
      for loc in xrange(location, location + size - self.PointerSize()):
        if reader._ReadWord(loc) == word:
          slot = start + (loc - location)
          print "%s: %s" % (reader.FormatIntPtr(slot),
                            reader.FormatIntPtr(word))
    self.ForEachMemoryRegion(search_inside_region)

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
    def CountUndefinedInstructions(lines):
      pattern = "<UNDEFINED>"
      return sum([line.count(pattern) for (ignore, line) in lines])

    location = self.FindLocation(address)
    if location is None: return []
    arch = None
    possible_objdump_flags = [""]
    if self.arch == MD_CPU_ARCHITECTURE_X86:
      arch = "ia32"
    elif self.arch == MD_CPU_ARCHITECTURE_ARM:
      arch = "arm"
      possible_objdump_flags = ["", "--disassembler-options=force-thumb"]
    elif self.arch == MD_CPU_ARCHITECTURE_AMD64:
      arch = "x64"
    results = [ disasm.GetDisasmLines(self.minidump_name,
                                     location,
                                     size,
                                     arch,
                                     False,
                                     objdump_flags)
                for objdump_flags in possible_objdump_flags ]
    return min(results, key=CountUndefinedInstructions)


  def Dispose(self):
    self.minidump.close()
    self.minidump_file.close()

  def ExceptionIP(self):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return self.exception_context.rip
    elif self.arch == MD_CPU_ARCHITECTURE_ARM:
      return self.exception_context.pc
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return self.exception_context.eip

  def ExceptionSP(self):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return self.exception_context.rsp
    elif self.arch == MD_CPU_ARCHITECTURE_ARM:
      return self.exception_context.sp
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return self.exception_context.esp

  def ExceptionFP(self):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return self.exception_context.rbp
    elif self.arch == MD_CPU_ARCHITECTURE_ARM:
      return None
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return self.exception_context.ebp

  def FormatIntPtr(self, value):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return "%016x" % value
    elif self.arch == MD_CPU_ARCHITECTURE_ARM:
      return "%08x" % value
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return "%08x" % value

  def PointerSize(self):
    if self.arch == MD_CPU_ARCHITECTURE_AMD64:
      return 8
    elif self.arch == MD_CPU_ARCHITECTURE_ARM:
      return 4
    elif self.arch == MD_CPU_ARCHITECTURE_X86:
      return 4

  def Register(self, name):
    return self.exception_context.__getattribute__(name)

  def ReadMinidumpString(self, rva):
    string = bytearray(MINIDUMP_STRING.Read(self.minidump, rva).buffer)
    string = string.decode("utf16")
    return string[0:len(string) - 1]

  # Load FUNC records from a BreakPad symbol file
  #
  #    http://code.google.com/p/google-breakpad/wiki/SymbolFiles
  #
  def _LoadSymbolsFrom(self, symfile, baseaddr):
    print "Loading symbols from %s" % (symfile)
    funcs = []
    with open(symfile) as f:
      for line in f:
        result = re.match(
            r"^FUNC ([a-f0-9]+) ([a-f0-9]+) ([a-f0-9]+) (.*)$", line)
        if result is not None:
          start = int(result.group(1), 16)
          size = int(result.group(2), 16)
          name = result.group(4).rstrip()
          bisect.insort_left(self.symbols,
                             FuncSymbol(baseaddr + start, size, name))
    print " ... done"

  def TryLoadSymbolsFor(self, modulename, module):
    try:
      symfile = os.path.join(self.symdir,
                             modulename.replace('.', '_') + ".pdb.sym")
      if os.path.isfile(symfile):
        self._LoadSymbolsFrom(symfile, module.base_of_image)
        self.modules_with_symbols.append(module)
    except Exception as e:
      print "  ... failure (%s)" % (e)

  # Returns true if address is covered by some module that has loaded symbols.
  def _IsInModuleWithSymbols(self, addr):
    for module in self.modules_with_symbols:
      start = module.base_of_image
      end = start + module.size_of_image
      if (start <= addr) and (addr < end):
        return True
    return False

  # Find symbol covering the given address and return its name in format
  #     <symbol name>+<offset from the start>
  def FindSymbol(self, addr):
    if not self._IsInModuleWithSymbols(addr):
      return None

    i = bisect.bisect_left(self.symbols, addr)
    symbol = None
    if (0 < i) and self.symbols[i - 1].Covers(addr):
      symbol = self.symbols[i - 1]
    elif (i < len(self.symbols)) and self.symbols[i].Covers(addr):
      symbol = self.symbols[i]
    else:
      return None
    diff = addr - symbol.start
    return "%s+0x%x" % (symbol.name, diff)


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
  def Decode(self, offset, size, value):
    return (value >> offset) & ((1 << size) - 1)

  # Instance Sizes
  def InstanceSizesOffset(self):
    return self.heap.PointerSize()

  def InstanceSizeOffset(self):
    return self.InstanceSizesOffset()

  def InObjectProperties(self):
    return self.InstanceSizeOffset() + 1

  def PreAllocatedPropertyFields(self):
    return self.InObjectProperties() + 1

  def VisitorId(self):
    return self.PreAllocatedPropertyFields() + 1

  # Instance Attributes
  def InstanceAttributesOffset(self):
    return self.InstanceSizesOffset() + self.heap.IntSize()

  def InstanceTypeOffset(self):
    return self.InstanceAttributesOffset()

  def UnusedPropertyFieldsOffset(self):
    return self.InstanceTypeOffset() + 1

  def BitFieldOffset(self):
    return self.UnusedPropertyFieldsOffset() + 1

  def BitField2Offset(self):
    return self.BitFieldOffset() + 1

  # Other fields
  def PrototypeOffset(self):
    return self.InstanceAttributesOffset() + self.heap.IntSize()

  def ConstructorOffset(self):
    return self.PrototypeOffset() + self.heap.PointerSize()

  def TransitionsOrBackPointerOffset(self):
    return self.ConstructorOffset() + self.heap.PointerSize()

  def DescriptorsOffset(self):
    return self.TransitionsOrBackPointerOffset() + self.heap.PointerSize()

  def CodeCacheOffset(self):
    return self.DescriptorsOffset() + self.heap.PointerSize()

  def DependentCodeOffset(self):
    return self.CodeCacheOffset() + self.heap.PointerSize()

  def BitField3Offset(self):
    return self.DependentCodeOffset() + self.heap.PointerSize()

  def ReadByte(self, offset):
    return self.heap.reader.ReadU8(self.address + offset)

  def Print(self, p):
    p.Print("Map(%08x)" % (self.address))
    p.Print("- size: %d, inobject: %d, preallocated: %d, visitor: %d" % (
        self.ReadByte(self.InstanceSizeOffset()),
        self.ReadByte(self.InObjectProperties()),
        self.ReadByte(self.PreAllocatedPropertyFields()),
        self.VisitorId()))

    bitfield = self.ReadByte(self.BitFieldOffset())
    bitfield2 = self.ReadByte(self.BitField2Offset())
    p.Print("- %s, unused: %d, bf: %d, bf2: %d" % (
        INSTANCE_TYPES[self.ReadByte(self.InstanceTypeOffset())],
        self.ReadByte(self.UnusedPropertyFieldsOffset()),
        bitfield, bitfield2))

    p.Print("- kind: %s" % (self.Decode(3, 5, bitfield2)))

    bitfield3 = self.ObjectField(self.BitField3Offset())
    p.Print(
        "- EnumLength: %d NumberOfOwnDescriptors: %d OwnsDescriptors: %s" % (
            self.Decode(0, 11, bitfield3),
            self.Decode(11, 11, bitfield3),
            self.Decode(25, 1, bitfield3)))
    p.Print("- IsShared: %s" % (self.Decode(22, 1, bitfield3)))
    p.Print("- FunctionWithPrototype: %s" % (self.Decode(23, 1, bitfield3)))
    p.Print("- DictionaryMap: %s" % (self.Decode(24, 1, bitfield3)))

    descriptors = self.ObjectField(self.DescriptorsOffset())
    if descriptors.__class__ == FixedArray:
      DescriptorArray(descriptors).Print(p)
    else:
      p.Print("Descriptors: %s" % (descriptors))

    transitions = self.ObjectField(self.TransitionsOrBackPointerOffset())
    if transitions.__class__ == FixedArray:
      TransitionArray(transitions).Print(p)
    else:
      p.Print("TransitionsOrBackPointer: %s" % (transitions))

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
    try:
      return self.left.GetChars() + self.right.GetChars()
    except:
      return "***CAUGHT EXCEPTION IN GROKDUMP***"


class Oddball(HeapObject):
  # Should match declarations in objects.h
  KINDS = [
    "False",
    "True",
    "TheHole",
    "Null",
    "ArgumentMarker",
    "Undefined",
    "Other"
  ]

  def ToStringOffset(self):
    return self.heap.PointerSize()

  def ToNumberOffset(self):
    return self.ToStringOffset() + self.heap.PointerSize()

  def KindOffset(self):
    return self.ToNumberOffset() + self.heap.PointerSize()

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.to_string = self.ObjectField(self.ToStringOffset())
    self.kind = self.SmiField(self.KindOffset())

  def Print(self, p):
    p.Print(str(self))

  def __str__(self):
    if self.to_string:
      return "Oddball(%08x, <%s>)" % (self.address, str(self.to_string))
    else:
      kind = "???"
      if 0 <= self.kind < len(Oddball.KINDS):
        kind = Oddball.KINDS[self.kind]
      return "Oddball(%08x, kind=%s)" % (self.address, kind)


class FixedArray(HeapObject):
  def LengthOffset(self):
    return self.heap.PointerSize()

  def ElementsOffset(self):
    return self.heap.PointerSize() * 2

  def MemberOffset(self, i):
    return self.ElementsOffset() + self.heap.PointerSize() * i

  def Get(self, i):
    return self.ObjectField(self.MemberOffset(i))

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
      try:
        p.Print("[%08d] = %s" % (i, self.ObjectField(offset)))
      except TypeError:
        p.Dedent()
        p.Print("...")
        p.Print("}")
        return
    p.Dedent()
    p.Print("}")

  def __str__(self):
    return "FixedArray(%08x, length=%d)" % (self.address, self.length)


class DescriptorArray(object):
  def __init__(self, array):
    self.array = array

  def Length(self):
    return self.array.Get(0)

  def Decode(self, offset, size, value):
    return (value >> offset) & ((1 << size) - 1)

  TYPES = [
      "normal",
      "field",
      "function",
      "callbacks"
  ]

  def Type(self, value):
    return DescriptorArray.TYPES[self.Decode(0, 3, value)]

  def Attributes(self, value):
    attributes = self.Decode(3, 3, value)
    result = []
    if (attributes & 0): result += ["ReadOnly"]
    if (attributes & 1): result += ["DontEnum"]
    if (attributes & 2): result += ["DontDelete"]
    return "[" + (",".join(result)) + "]"

  def Deleted(self, value):
    return self.Decode(6, 1, value) == 1

  def Storage(self, value):
    return self.Decode(7, 11, value)

  def Pointer(self, value):
    return self.Decode(18, 11, value)

  def Details(self, di, value):
    return (
        di,
        self.Type(value),
        self.Attributes(value),
        self.Storage(value),
        self.Pointer(value)
    )


  def Print(self, p):
    length = self.Length()
    array = self.array

    p.Print("Descriptors(%08x, length=%d)" % (array.address, length))
    p.Print("[et] %s" % (array.Get(1)))

    for di in xrange(length):
      i = 2 + di * 3
      p.Print("0x%x" % (array.address + array.MemberOffset(i)))
      p.Print("[%i] name:    %s" % (di, array.Get(i + 0)))
      p.Print("[%i] details: %s %s enum %i pointer %i" % \
              self.Details(di, array.Get(i + 1)))
      p.Print("[%i] value:   %s" % (di, array.Get(i + 2)))

    end = self.array.length // 3
    if length != end:
      p.Print("[%i-%i] slack descriptors" % (length, end))


class TransitionArray(object):
  def __init__(self, array):
    self.array = array

  def IsSimpleTransition(self):
    return self.array.length <= 2

  def Length(self):
    # SimpleTransition cases
    if self.IsSimpleTransition():
      return self.array.length - 1
    return (self.array.length - 3) // 2

  def Print(self, p):
    length = self.Length()
    array = self.array

    p.Print("Transitions(%08x, length=%d)" % (array.address, length))
    p.Print("[backpointer] %s" % (array.Get(0)))
    if self.IsSimpleTransition():
      if length == 1:
        p.Print("[simple target] %s" % (array.Get(1)))
      return

    elements = array.Get(1)
    if elements is not None:
      p.Print("[elements   ] %s" % (elements))

    prototype = array.Get(2)
    if prototype is not None:
      p.Print("[prototype  ] %s" % (prototype))

    for di in xrange(length):
      i = 3 + di * 2
      p.Print("[%i] symbol: %s" % (di, array.Get(i + 0)))
      p.Print("[%i] target: %s" % (di, array.Get(i + 1)))


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


class CodeCache(HeapObject):
  def DefaultCacheOffset(self):
    return self.heap.PointerSize()

  def NormalTypeCacheOffset(self):
    return self.DefaultCacheOffset() + self.heap.PointerSize()

  def __init__(self, heap, map, address):
    HeapObject.__init__(self, heap, map, address)
    self.default_cache = self.ObjectField(self.DefaultCacheOffset())
    self.normal_type_cache = self.ObjectField(self.NormalTypeCacheOffset())

  def Print(self, p):
    p.Print("CodeCache(%s) {" % self.heap.reader.FormatIntPtr(self.address))
    p.Indent()
    p.Print("default cache: %s" % self.default_cache)
    p.Print("normal type cache: %s" % self.normal_type_cache)
    p.Dedent()
    p.Print("}")


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
    "CODE_CACHE_TYPE": CodeCache,
    "CODE_TYPE": Code,
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
    elif self.reader.arch == MD_CPU_ARCHITECTURE_ARM:
      return (1 << 4) - 1
    elif self.reader.arch == MD_CPU_ARCHITECTURE_X86:
      return (1 << 5) - 1

  def PageAlignmentMask(self):
    return (1 << 20) - 1


class KnownObject(HeapObject):
  def __init__(self, heap, known_name):
    HeapObject.__init__(self, heap, None, None)
    self.known_name = known_name

  def __str__(self):
    return "<%s>" % self.known_name


class KnownMap(HeapObject):
  def __init__(self, heap, known_name, instance_type):
    HeapObject.__init__(self, heap, None, None)
    self.instance_type = instance_type
    self.known_name = known_name

  def __str__(self):
    return "<%s>" % self.known_name


class InspectionPadawan(object):
  """The padawan can improve annotations by sensing well-known objects."""
  def __init__(self, reader, heap):
    self.reader = reader
    self.heap = heap
    self.known_first_map_page = 0
    self.known_first_data_page = 0
    self.known_first_pointer_page = 0

  def __getattr__(self, name):
    """An InspectionPadawan can be used instead of V8Heap, even though
       it does not inherit from V8Heap (aka. mixin)."""
    return getattr(self.heap, name)

  def GetPageOffset(self, tagged_address):
    return tagged_address & self.heap.PageAlignmentMask()

  def IsInKnownMapSpace(self, tagged_address):
    page_address = tagged_address & ~self.heap.PageAlignmentMask()
    return page_address == self.known_first_map_page

  def IsInKnownOldSpace(self, tagged_address):
    page_address = tagged_address & ~self.heap.PageAlignmentMask()
    return page_address in [self.known_first_data_page,
                            self.known_first_pointer_page]

  def ContainingKnownOldSpaceName(self, tagged_address):
    page_address = tagged_address & ~self.heap.PageAlignmentMask()
    if page_address == self.known_first_data_page: return "OLD_DATA_SPACE"
    if page_address == self.known_first_pointer_page: return "OLD_POINTER_SPACE"
    return None

  def SenseObject(self, tagged_address):
    if self.IsInKnownOldSpace(tagged_address):
      offset = self.GetPageOffset(tagged_address)
      lookup_key = (self.ContainingKnownOldSpaceName(tagged_address), offset)
      known_obj_name = KNOWN_OBJECTS.get(lookup_key)
      if known_obj_name:
        return KnownObject(self, known_obj_name)
    if self.IsInKnownMapSpace(tagged_address):
      known_map = self.SenseMap(tagged_address)
      if known_map:
        return known_map
    found_obj = self.heap.FindObject(tagged_address)
    if found_obj: return found_obj
    address = tagged_address - 1
    if self.reader.IsValidAddress(address):
      map_tagged_address = self.reader.ReadUIntPtr(address)
      map = self.SenseMap(map_tagged_address)
      if map is None: return None
      instance_type_name = INSTANCE_TYPES.get(map.instance_type)
      if instance_type_name is None: return None
      cls = V8Heap.CLASS_MAP.get(instance_type_name, HeapObject)
      return cls(self, map, address)
    return None

  def SenseMap(self, tagged_address):
    if self.IsInKnownMapSpace(tagged_address):
      offset = self.GetPageOffset(tagged_address)
      known_map_info = KNOWN_MAPS.get(offset)
      if known_map_info:
        known_map_type, known_map_name = known_map_info
        return KnownMap(self, known_map_name, known_map_type)
    found_map = self.heap.FindMap(tagged_address)
    if found_map: return found_map
    return None

  def FindObjectOrSmi(self, tagged_address):
    """When used as a mixin in place of V8Heap."""
    found_obj = self.SenseObject(tagged_address)
    if found_obj: return found_obj
    if (tagged_address & 1) == 0:
      return "Smi(%d)" % (tagged_address / 2)
    else:
      return "Unknown(%s)" % self.reader.FormatIntPtr(tagged_address)

  def FindObject(self, tagged_address):
    """When used as a mixin in place of V8Heap."""
    raise NotImplementedError

  def FindMap(self, tagged_address):
    """When used as a mixin in place of V8Heap."""
    raise NotImplementedError

  def PrintKnowledge(self):
    print "  known_first_map_page = %s\n"\
          "  known_first_data_page = %s\n"\
          "  known_first_pointer_page = %s" % (
          self.reader.FormatIntPtr(self.known_first_map_page),
          self.reader.FormatIntPtr(self.known_first_data_page),
          self.reader.FormatIntPtr(self.known_first_pointer_page))


class InspectionShell(cmd.Cmd):
  def __init__(self, reader, heap):
    cmd.Cmd.__init__(self)
    self.reader = reader
    self.heap = heap
    self.padawan = InspectionPadawan(reader, heap)
    self.prompt = "(grok) "

  def do_da(self, address):
    """
     Print ASCII string starting at specified address.
    """
    address = int(address, 16)
    string = ""
    while self.reader.IsValidAddress(address):
      code = self.reader.ReadU8(address)
      if code < 128:
        string += chr(code)
      else:
        break
      address += 1
    if string == "":
      print "Not an ASCII string at %s" % self.reader.FormatIntPtr(address)
    else:
      print "%s\n" % string

  def do_dd(self, address):
    """
     Interpret memory at the given address (if available) as a sequence
     of words. Automatic alignment is not performed.
    """
    start = int(address, 16)
    if (start & self.heap.ObjectAlignmentMask()) != 0:
      print "Warning: Dumping un-aligned memory, is this what you had in mind?"
    for slot in xrange(start,
                       start + self.reader.PointerSize() * 10,
                       self.reader.PointerSize()):
      if not self.reader.IsValidAddress(slot):
        print "Address is not contained within the minidump!"
        return
      maybe_address = self.reader.ReadUIntPtr(slot)
      heap_object = self.padawan.SenseObject(maybe_address)
      print "%s: %s %s" % (self.reader.FormatIntPtr(slot),
                           self.reader.FormatIntPtr(maybe_address),
                           heap_object or '')

  def do_do(self, address):
    """
     Interpret memory at the given address as a V8 object. Automatic
     alignment makes sure that you can pass tagged as well as un-tagged
     addresses.
    """
    address = int(address, 16)
    if (address & self.heap.ObjectAlignmentMask()) == 0:
      address = address + 1
    elif (address & self.heap.ObjectAlignmentMask()) != 1:
      print "Address doesn't look like a valid pointer!"
      return
    heap_object = self.padawan.SenseObject(address)
    if heap_object:
      heap_object.Print(Printer())
    else:
      print "Address cannot be interpreted as object!"

  def do_do_desc(self, address):
    """
      Print a descriptor array in a readable format.
    """
    start = int(address, 16)
    if ((start & 1) == 1): start = start - 1
    DescriptorArray(FixedArray(self.heap, None, start)).Print(Printer())

  def do_do_map(self, address):
    """
      Print a descriptor array in a readable format.
    """
    start = int(address, 16)
    if ((start & 1) == 1): start = start - 1
    Map(self.heap, None, start).Print(Printer())

  def do_do_trans(self, address):
    """
      Print a transition array in a readable format.
    """
    start = int(address, 16)
    if ((start & 1) == 1): start = start - 1
    TransitionArray(FixedArray(self.heap, None, start)).Print(Printer())

  def do_dp(self, address):
    """
     Interpret memory at the given address as being on a V8 heap page
     and print information about the page header (if available).
    """
    address = int(address, 16)
    page_address = address & ~self.heap.PageAlignmentMask()
    if self.reader.IsValidAddress(page_address):
      raise NotImplementedError
    else:
      print "Page header is not available!"

  def do_k(self, arguments):
    """
     Teach V8 heap layout information to the inspector. This increases
     the amount of annotations the inspector can produce while dumping
     data. The first page of each heap space is of particular interest
     because it contains known objects that do not move.
    """
    self.padawan.PrintKnowledge()

  def do_kd(self, address):
    """
     Teach V8 heap layout information to the inspector. Set the first
     data-space page by passing any pointer into that page.
    """
    address = int(address, 16)
    page_address = address & ~self.heap.PageAlignmentMask()
    self.padawan.known_first_data_page = page_address

  def do_km(self, address):
    """
     Teach V8 heap layout information to the inspector. Set the first
     map-space page by passing any pointer into that page.
    """
    address = int(address, 16)
    page_address = address & ~self.heap.PageAlignmentMask()
    self.padawan.known_first_map_page = page_address

  def do_kp(self, address):
    """
     Teach V8 heap layout information to the inspector. Set the first
     pointer-space page by passing any pointer into that page.
    """
    address = int(address, 16)
    page_address = address & ~self.heap.PageAlignmentMask()
    self.padawan.known_first_pointer_page = page_address

  def do_list(self, smth):
    """
     List all available memory regions.
    """
    def print_region(reader, start, size, location):
      print "  %s - %s (%d bytes)" % (reader.FormatIntPtr(start),
                                      reader.FormatIntPtr(start + size),
                                      size)
    print "Available memory regions:"
    self.reader.ForEachMemoryRegion(print_region)

  def do_lm(self, arg):
    """
     List details for all loaded modules in the minidump. An argument can
     be passed to limit the output to only those modules that contain the
     argument as a substring (case insensitive match).
    """
    for module in self.reader.module_list.modules:
      if arg:
        name = GetModuleName(self.reader, module).lower()
        if name.find(arg.lower()) >= 0:
          PrintModuleDetails(self.reader, module)
      else:
        PrintModuleDetails(self.reader, module)
    print

  def do_s(self, word):
    """
     Search for a given word in available memory regions. The given word
     is expanded to full pointer size and searched at aligned as well as
     un-aligned memory locations. Use 'sa' to search aligned locations
     only.
    """
    try:
      word = int(word, 0)
    except ValueError:
      print "Malformed word, prefix with '0x' to use hexadecimal format."
      return
    print "Searching for word %d/0x%s:" % (word, self.reader.FormatIntPtr(word))
    self.reader.FindWord(word)

  def do_sh(self, none):
    """
     Search for the V8 Heap object in all available memory regions. You
     might get lucky and find this rare treasure full of invaluable
     information.
    """
    raise NotImplementedError

  def do_u(self, args):
    """
     Unassemble memory in the region [address, address + size). If the
     size is not specified, a default value of 32 bytes is used.
     Synopsis: u 0x<address> 0x<size>
    """
    args = args.split(' ')
    start = int(args[0], 16)
    size = int(args[1], 16) if len(args) > 1 else 0x20
    if not self.reader.IsValidAddress(start):
      print "Address is not contained within the minidump!"
      return
    lines = self.reader.GetDisasmLines(start, size)
    for line in lines:
      print FormatDisasmLine(start, self.heap, line)
    print

  def do_EOF(self, none):
    raise KeyboardInterrupt

EIP_PROXIMITY = 64

CONTEXT_FOR_ARCH = {
    MD_CPU_ARCHITECTURE_AMD64:
      ['rax', 'rbx', 'rcx', 'rdx', 'rdi', 'rsi', 'rbp', 'rsp', 'rip',
       'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15'],
    MD_CPU_ARCHITECTURE_ARM:
      ['r0', 'r1', 'r2', 'r3', 'r4', 'r5', 'r6', 'r7', 'r8', 'r9',
       'r10', 'r11', 'r12', 'sp', 'lr', 'pc'],
    MD_CPU_ARCHITECTURE_X86:
      ['eax', 'ebx', 'ecx', 'edx', 'edi', 'esi', 'ebp', 'esp', 'eip']
}

KNOWN_MODULES = {'chrome.exe', 'chrome.dll'}

def GetVersionString(ms, ls):
  return "%d.%d.%d.%d" % (ms >> 16, ms & 0xffff, ls >> 16, ls & 0xffff)


def GetModuleName(reader, module):
  name = reader.ReadMinidumpString(module.module_name_rva)
  # simplify for path manipulation
  name = name.encode('utf-8')
  return str(os.path.basename(str(name).replace("\\", "/")))


def PrintModuleDetails(reader, module):
  print "%s" % GetModuleName(reader, module)
  file_version = GetVersionString(module.version_info.dwFileVersionMS,
                                  module.version_info.dwFileVersionLS)
  product_version = GetVersionString(module.version_info.dwProductVersionMS,
                                     module.version_info.dwProductVersionLS)
  print "  base: %s" % reader.FormatIntPtr(module.base_of_image)
  print "  end: %s" % reader.FormatIntPtr(module.base_of_image +
                                          module.size_of_image)
  print "  file version: %s" % file_version
  print "  product version: %s" % product_version
  time_date_stamp = datetime.datetime.fromtimestamp(module.time_date_stamp)
  print "  timestamp: %s" % time_date_stamp


def AnalyzeMinidump(options, minidump_name):
  reader = MinidumpReader(options, minidump_name)
  heap = None
  DebugPrint("========================================")
  if reader.exception is None:
    print "Minidump has no exception info"
  else:
    print "Exception info:"
    exception_thread = reader.thread_map[reader.exception.thread_id]
    print "  thread id: %d" % exception_thread.id
    print "  code: %08X" % reader.exception.exception.code
    print "  context:"
    for r in CONTEXT_FOR_ARCH[reader.arch]:
      print "    %s: %s" % (r, reader.FormatIntPtr(reader.Register(r)))
    # TODO(vitalyr): decode eflags.
    if reader.arch == MD_CPU_ARCHITECTURE_ARM:
      print "    cpsr: %s" % bin(reader.exception_context.cpsr)[2:]
    else:
      print "    eflags: %s" % bin(reader.exception_context.eflags)[2:]

    print
    print "  modules:"
    for module in reader.module_list.modules:
      name = GetModuleName(reader, module)
      if name in KNOWN_MODULES:
        print "    %s at %08X" % (name, module.base_of_image)
        reader.TryLoadSymbolsFor(name, module)
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
    eip_symbol = reader.FindSymbol(reader.ExceptionIP())
    if eip_symbol is not None:
      print eip_symbol
    disasm_start = reader.ExceptionIP() - EIP_PROXIMITY
    disasm_bytes = 2 * EIP_PROXIMITY
    if (options.full):
      full_range = reader.FindRegion(reader.ExceptionIP())
      if full_range is not None:
        disasm_start = full_range[0]
        disasm_bytes = full_range[1]

    lines = reader.GetDisasmLines(disasm_start, disasm_bytes)

    for line in lines:
      print FormatDisasmLine(disasm_start, heap, line)
    print

  if heap is None:
    heap = V8Heap(reader, None)

  if options.full:
    FullDump(reader, heap)

  if options.command:
    InspectionShell(reader, heap).onecmd(options.command)

  if options.shell:
    try:
      InspectionShell(reader, heap).cmdloop("type help to get help")
    except KeyboardInterrupt:
      print "Kthxbye."
  elif not options.command:
    if reader.exception is not None:
      frame_pointer = reader.ExceptionFP()
      print "Annotated stack (from exception.esp to bottom):"
      for slot in xrange(stack_top, stack_bottom, reader.PointerSize()):
        maybe_address = reader.ReadUIntPtr(slot)
        heap_object = heap.FindObject(maybe_address)
        maybe_symbol = reader.FindSymbol(maybe_address)
        if slot == frame_pointer:
          maybe_symbol = "<---- frame pointer"
          frame_pointer = maybe_address
        print "%s: %s %s" % (reader.FormatIntPtr(slot),
                             reader.FormatIntPtr(maybe_address),
                             maybe_symbol or "")
        if heap_object:
          heap_object.Print(Printer())
          print

  reader.Dispose()


if __name__ == "__main__":
  parser = optparse.OptionParser(USAGE)
  parser.add_option("-s", "--shell", dest="shell", action="store_true",
                    help="start an interactive inspector shell")
  parser.add_option("-c", "--command", dest="command", default="",
                    help="run an interactive inspector shell command and exit")
  parser.add_option("-f", "--full", dest="full", action="store_true",
                    help="dump all information contained in the minidump")
  parser.add_option("--symdir", dest="symdir", default=".",
                    help="directory containing *.pdb.sym file with symbols")
  parser.add_option("--objdump",
                    default="/usr/bin/objdump",
                    help="objdump tool to use [default: %default]")
  options, args = parser.parse_args()
  if os.path.exists(options.objdump):
    disasm.OBJDUMP_BIN = options.objdump
    OBJDUMP_BIN = options.objdump
  else:
    print "Cannot find %s, falling back to default objdump" % options.objdump
  if len(args) != 1:
    parser.print_help()
    sys.exit(1)
  AnalyzeMinidump(options, args[0])
