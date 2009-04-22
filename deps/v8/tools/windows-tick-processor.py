#!/usr/bin/env python
#
# Copyright 2008 the V8 project authors. All rights reserved.
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


# Usage: process-ticks.py <binary> <logfile>
#
# Where <binary> is the binary program name (eg, v8_shell.exe) and
# <logfile> is the log file name (eg, v8.log).
#
# This tick processor expects to find a map file for the binary named
# binary.map if the binary is named binary.exe. The tick processor
# only works for statically linked executables - no information about
# shared libraries is logged from v8 on Windows.

import os, re, sys, tickprocessor

class WindowsTickProcessor(tickprocessor.TickProcessor):

  def Unmangle(self, name):
    """Performs very simple unmangling of C++ names.

    Does not handle arguments and template arguments. The mangled names have
    the form:

       ?LookupInDescriptor@JSObject@internal@v8@@...arguments info...

    """
    # Name is mangled if it starts with a question mark.
    is_mangled = re.match("^\?(.*)", name)
    if is_mangled:
      substrings = is_mangled.group(1).split('@')
      try:
        # The function name is terminated by two @s in a row.  Find the
        # substrings that are part of the function name.
        index = substrings.index('')
        substrings = substrings[0:index]
      except ValueError:
        # If we did not find two @s in a row, the mangled name is not in
        # the format we expect and we give up.
        return name
      substrings.reverse()
      function_name = "::".join(substrings)
      return function_name
    return name


  def ParseMapFile(self, filename):
    """Parse map file and add symbol information to the cpp entries."""
    # Locate map file.
    has_dot = re.match('^([a-zA-F0-9_-]*)[\.]?.*$', filename)
    if has_dot:
      map_file_name = has_dot.group(1) + '.map'
      try:
        map_file = open(map_file_name, 'rb')
      except IOError:
        sys.exit("Could not open map file: " + map_file_name)
    else:
      sys.exit("Could not find map file for executable: " + filename)
    try:
      max_addr = 0
      min_addr = 2**30
      # Process map file and search for function entries.
      row_regexp = re.compile(' 0001:[0-9a-fA-F]{8}\s*([_\?@$0-9a-zA-Z]*)\s*([0-9a-fA-F]{8}).*')
      for line in map_file:
        row = re.match(row_regexp, line)
        if row:
          addr = int(row.group(2), 16)
          if addr > max_addr:
            max_addr = addr
          if addr < min_addr:
            min_addr = addr
          mangled_name = row.group(1)
          name = self.Unmangle(mangled_name)
          self.cpp_entries.Insert(addr, tickprocessor.CodeEntry(addr, name));
      i = min_addr
      # Mark the pages for which there are functions in the map file.
      while i < max_addr:
        page = i >> 12
        self.vm_extent[page] = 1
        i += 4096
    finally:
      map_file.close()


class WindowsCmdLineProcessor(tickprocessor.CmdLineProcessor):

  def __init__(self):
    super(WindowsCmdLineProcessor, self).__init__()
    self.binary_file = None

  def GetRequiredArgsNames(self):
    return 'binary log_file'

  def ProcessRequiredArgs(self, args):
    if len(args) != 2:
      self.PrintUsageAndExit()
    else:
      self.binary_file = args[0]
      self.log_file = args[1]


def Main():
  cmdline_processor = WindowsCmdLineProcessor()
  cmdline_processor.ProcessArguments()
  tickprocessor = WindowsTickProcessor()
  tickprocessor.ParseMapFile(cmdline_processor.binary_file)
  cmdline_processor.RunLogfileProcessing(tickprocessor)
  tickprocessor.PrintResults()

if __name__ == '__main__':
  Main()
