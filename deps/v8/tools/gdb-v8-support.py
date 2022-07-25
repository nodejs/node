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

# for py2/py3 compatibility
from __future__ import print_function

import re
import tempfile
import os
import subprocess
import time
import gdb

kSmiTag = 0
kSmiTagSize = 1
kSmiTagMask = (1 << kSmiTagSize) - 1

kHeapObjectTag = 1
kHeapObjectTagSize = 2
kHeapObjectTagMask = (1 << kHeapObjectTagSize) - 1

kFailureTag = 3
kFailureTagSize = 2
kFailureTagMask = (1 << kFailureTagSize) - 1

kSmiShiftSize32 = 0
kSmiValueSize32 = 31
kSmiShiftBits32 = kSmiTagSize + kSmiShiftSize32

kSmiShiftSize64 = 31
kSmiValueSize64 = 32
kSmiShiftBits64 = kSmiTagSize + kSmiShiftSize64

kAllBits = 0xFFFFFFFF
kTopBit32 = 0x80000000
kTopBit64 = 0x8000000000000000

t_u32 = gdb.lookup_type('unsigned int')
t_u64 = gdb.lookup_type('unsigned long long')


def has_smi_tag(v):
  return v & kSmiTagMask == kSmiTag


def has_failure_tag(v):
  return v & kFailureTagMask == kFailureTag


def has_heap_object_tag(v):
  return v & kHeapObjectTagMask == kHeapObjectTag


def raw_heap_object(v):
  return v - kHeapObjectTag


def smi_to_int_32(v):
  v = v & kAllBits
  if (v & kTopBit32) == kTopBit32:
    return ((v & kAllBits) >> kSmiShiftBits32) - 2147483648
  else:
    return (v & kAllBits) >> kSmiShiftBits32


def smi_to_int_64(v):
  return (v >> kSmiShiftBits64)


def decode_v8_value(v, bitness):
  base_str = 'v8[%x]' % v
  if has_smi_tag(v):
    if bitness == 32:
      return base_str + (" SMI(%d)" % smi_to_int_32(v))
    else:
      return base_str + (" SMI(%d)" % smi_to_int_64(v))
  elif has_failure_tag(v):
    return base_str + " (failure)"
  elif has_heap_object_tag(v):
    return base_str + (" H(0x%x)" % raw_heap_object(v))
  else:
    return base_str


class V8ValuePrinter(object):
  "Print a v8value."

  def __init__(self, val):
    self.val = val

  def to_string(self):
    if self.val.type.sizeof == 4:
      v_u32 = self.val.cast(t_u32)
      return decode_v8_value(int(v_u32), 32)
    elif self.val.type.sizeof == 8:
      v_u64 = self.val.cast(t_u64)
      return decode_v8_value(int(v_u64), 64)
    else:
      return 'v8value?'

  def display_hint(self):
    return 'v8value'


def v8_pretty_printers(val):
  lookup_tag = val.type.tag
  if lookup_tag == None:
    return None
  elif lookup_tag == 'v8value':
    return V8ValuePrinter(val)
  return None


gdb.pretty_printers.append(v8_pretty_printers)


def v8_to_int(v):
  if v.type.sizeof == 4:
    return int(v.cast(t_u32))
  elif v.type.sizeof == 8:
    return int(v.cast(t_u64))
  else:
    return '?'


def v8_get_value(vstring):
  v = gdb.parse_and_eval(vstring)
  return v8_to_int(v)


class V8PrintObject(gdb.Command):
  """Prints a v8 object."""
  def __init__(self):
    super(V8PrintObject, self).__init__("v8print", gdb.COMMAND_DATA)

  def invoke(self, arg, from_tty):
    v = v8_get_value(arg)
    gdb.execute('call __gdb_print_v8_object(%d)' % v)


V8PrintObject()


class FindAnywhere(gdb.Command):
  """Search memory for the given pattern."""
  MAPPING_RE = re.compile(r"^\s*\[\d+\]\s+0x([0-9A-Fa-f]+)->0x([0-9A-Fa-f]+)")
  LIVE_MAPPING_RE = re.compile(r"^\s+0x([0-9A-Fa-f]+)\s+0x([0-9A-Fa-f]+)")

  def __init__(self):
    super(FindAnywhere, self).__init__("find-anywhere", gdb.COMMAND_DATA)

  def find(self, startAddr, endAddr, value):
    try:
      result = gdb.execute("find 0x%s, 0x%s, %s" % (startAddr, endAddr, value),
                           to_string=True)
      if result.find("not found") == -1:
        print(result)
    except:
      pass

  def invoke(self, value, from_tty):
    for l in gdb.execute("maint info sections", to_string=True).split('\n'):
      m = FindAnywhere.MAPPING_RE.match(l)
      if m is None:
        continue
      self.find(m.group(1), m.group(2), value)
    for l in gdb.execute("info proc mappings", to_string=True).split('\n'):
      m = FindAnywhere.LIVE_MAPPING_RE.match(l)
      if m is None:
        continue
      self.find(m.group(1), m.group(2), value)


FindAnywhere()


class Redirect(gdb.Command):
  """Redirect the subcommand's stdout  to a temporary file.

Usage:   redirect subcommand...
Example:
  redirect job 0x123456789
  redirect x/1024xg 0x12345678

If provided, the generated temporary file is directly openend with the
GDB_EXTERNAL_EDITOR environment variable.
  """
  def __init__(self):
    super(Redirect, self).__init__("redirect", gdb.COMMAND_USER)

  def invoke(self, subcommand, from_tty):
    old_stdout = gdb.execute("p (int)dup(1)",
                             to_string=True).split("=")[-1].strip()
    try:
      time_suffix = time.strftime("%Y%m%d-%H%M%S")
      fd, file = tempfile.mkstemp(suffix="-%s.gdbout" % time_suffix)
      try:
        # Temporarily redirect stdout to the created tmp file for the
        # duration of the subcommand.
        gdb.execute('p (int)dup2((int)open("%s", 1), 1)' % file,
                    to_string=True)
        # Execute subcommand non interactively.
        result = gdb.execute(subcommand, from_tty=False, to_string=True)
        # Write returned string results to the temporary file as well.
        with open(file, 'a') as f:
          f.write(result)
        # Open generated result.
        if 'GDB_EXTERNAL_EDITOR' in os.environ:
          open_cmd = os.environ['GDB_EXTERNAL_EDITOR']
          print("Opening '%s' with %s" % (file, open_cmd))
          subprocess.call([open_cmd, file])
        else:
          print("Output written to:\n '%s'" % file)
      finally:
        # Restore original stdout.
        gdb.execute("p (int)dup2(%s, 1)" % old_stdout, to_string=True)
        # Close the temporary file.
        os.close(fd)
    finally:
      # Close the originally duplicated stdout descriptor.
      gdb.execute("p (int)close(%s)" % old_stdout, to_string=True)


Redirect()
