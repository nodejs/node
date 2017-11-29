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


"""A cross-platform execution counter viewer.

The stats viewer reads counters from a binary file and displays them
in a window, re-reading and re-displaying with regular intervals.
"""

import mmap
import optparse
import os
import re
import struct
import sys
import time
import Tkinter


# The interval, in milliseconds, between ui updates
UPDATE_INTERVAL_MS = 100


# Mapping from counter prefix to the formatting to be used for the counter
COUNTER_LABELS = {"t": "%i ms.", "c": "%i"}


# The magic numbers used to check if a file is not a counters file
COUNTERS_FILE_MAGIC_NUMBER = 0xDEADFACE
CHROME_COUNTERS_FILE_MAGIC_NUMBER = 0x13131313


class StatsViewer(object):
  """The main class that keeps the data used by the stats viewer."""

  def __init__(self, data_name, name_filter):
    """Creates a new instance.

    Args:
      data_name: the name of the file containing the counters.
      name_filter: The regexp filter to apply to counter names.
    """
    self.data_name = data_name
    self.name_filter = name_filter

    # The handle created by mmap.mmap to the counters file.  We need
    # this to clean it up on exit.
    self.shared_mmap = None

    # A mapping from counter names to the ui element that displays
    # them
    self.ui_counters = {}

    # The counter collection used to access the counters file
    self.data = None

    # The Tkinter root window object
    self.root = None

  def Run(self):
    """The main entry-point to running the stats viewer."""
    try:
      self.data = self.MountSharedData()
      # OpenWindow blocks until the main window is closed
      self.OpenWindow()
    finally:
      self.CleanUp()

  def MountSharedData(self):
    """Mount the binary counters file as a memory-mapped file.  If
    something goes wrong print an informative message and exit the
    program."""
    if not os.path.exists(self.data_name):
      maps_name = "/proc/%s/maps" % self.data_name
      if not os.path.exists(maps_name):
        print "\"%s\" is neither a counter file nor a PID." % self.data_name
        sys.exit(1)
      maps_file = open(maps_name, "r")
      try:
        self.data_name = None
        for m in re.finditer(r"/dev/shm/\S*", maps_file.read()):
          if os.path.exists(m.group(0)):
            self.data_name = m.group(0)
            break
        if self.data_name is None:
          print "Can't find counter file in maps for PID %s." % self.data_name
          sys.exit(1)
      finally:
        maps_file.close()
    data_file = open(self.data_name, "r")
    size = os.fstat(data_file.fileno()).st_size
    fileno = data_file.fileno()
    self.shared_mmap = mmap.mmap(fileno, size, access=mmap.ACCESS_READ)
    data_access = SharedDataAccess(self.shared_mmap)
    if data_access.IntAt(0) == COUNTERS_FILE_MAGIC_NUMBER:
      return CounterCollection(data_access)
    elif data_access.IntAt(0) == CHROME_COUNTERS_FILE_MAGIC_NUMBER:
      return ChromeCounterCollection(data_access)
    print "File %s is not stats data." % self.data_name
    sys.exit(1)

  def CleanUp(self):
    """Cleans up the memory mapped file if necessary."""
    if self.shared_mmap:
      self.shared_mmap.close()

  def UpdateCounters(self):
    """Read the contents of the memory-mapped file and update the ui if
    necessary.  If the same counters are present in the file as before
    we just update the existing labels.  If any counters have been added
    or removed we scrap the existing ui and draw a new one.
    """
    changed = False
    counters_in_use = self.data.CountersInUse()
    if counters_in_use != len(self.ui_counters):
      self.RefreshCounters()
      changed = True
    else:
      for i in xrange(self.data.CountersInUse()):
        counter = self.data.Counter(i)
        name = counter.Name()
        if name in self.ui_counters:
          value = counter.Value()
          ui_counter = self.ui_counters[name]
          counter_changed = ui_counter.Set(value)
          changed = (changed or counter_changed)
        else:
          self.RefreshCounters()
          changed = True
          break
    if changed:
      # The title of the window shows the last time the file was
      # changed.
      self.UpdateTime()
    self.ScheduleUpdate()

  def UpdateTime(self):
    """Update the title of the window with the current time."""
    self.root.title("Stats Viewer [updated %s]" % time.strftime("%H:%M:%S"))

  def ScheduleUpdate(self):
    """Schedules the next ui update."""
    self.root.after(UPDATE_INTERVAL_MS, lambda: self.UpdateCounters())

  def RefreshCounters(self):
    """Tear down and rebuild the controls in the main window."""
    counters = self.ComputeCounters()
    self.RebuildMainWindow(counters)

  def ComputeCounters(self):
    """Group the counters by the suffix of their name.

    Since the same code-level counter (for instance "X") can result in
    several variables in the binary counters file that differ only by a
    two-character prefix (for instance "c:X" and "t:X") counters are
    grouped by suffix and then displayed with custom formatting
    depending on their prefix.

    Returns:
      A mapping from suffixes to a list of counters with that suffix,
      sorted by prefix.
    """
    names = {}
    for i in xrange(self.data.CountersInUse()):
      counter = self.data.Counter(i)
      name = counter.Name()
      names[name] = counter

    # By sorting the keys we ensure that the prefixes always come in the
    # same order ("c:" before "t:") which looks more consistent in the
    # ui.
    sorted_keys = names.keys()
    sorted_keys.sort()

    # Group together the names whose suffix after a ':' are the same.
    groups = {}
    for name in sorted_keys:
      counter = names[name]
      if ":" in name:
        name = name[name.find(":")+1:]
      if not name in groups:
        groups[name] = []
      groups[name].append(counter)

    return groups

  def RebuildMainWindow(self, groups):
    """Tear down and rebuild the main window.

    Args:
      groups: the groups of counters to display
    """
    # Remove elements in the current ui
    self.ui_counters.clear()
    for child in self.root.children.values():
      child.destroy()

    # Build new ui
    index = 0
    sorted_groups = groups.keys()
    sorted_groups.sort()
    for counter_name in sorted_groups:
      counter_objs = groups[counter_name]
      if self.name_filter.match(counter_name):
        name = Tkinter.Label(self.root, width=50, anchor=Tkinter.W,
                             text=counter_name)
        name.grid(row=index, column=0, padx=1, pady=1)
      count = len(counter_objs)
      for i in xrange(count):
        counter = counter_objs[i]
        name = counter.Name()
        var = Tkinter.StringVar()
        if self.name_filter.match(name):
          value = Tkinter.Label(self.root, width=15, anchor=Tkinter.W,
                                textvariable=var)
          value.grid(row=index, column=(1 + i), padx=1, pady=1)

        # If we know how to interpret the prefix of this counter then
        # add an appropriate formatting to the variable
        if (":" in name) and (name[0] in COUNTER_LABELS):
          format = COUNTER_LABELS[name[0]]
        else:
          format = "%i"
        ui_counter = UiCounter(var, format)
        self.ui_counters[name] = ui_counter
        ui_counter.Set(counter.Value())
      index += 1
    self.root.update()

  def OpenWindow(self):
    """Create and display the root window."""
    self.root = Tkinter.Tk()

    # Tkinter is no good at resizing so we disable it
    self.root.resizable(width=False, height=False)
    self.RefreshCounters()
    self.ScheduleUpdate()
    self.root.mainloop()


class UiCounter(object):
  """A counter in the ui."""

  def __init__(self, var, format):
    """Creates a new ui counter.

    Args:
      var: the Tkinter string variable for updating the ui
      format: the format string used to format this counter
    """
    self.var = var
    self.format = format
    self.last_value = None

  def Set(self, value):
    """Updates the ui for this counter.

    Args:
      value: The value to display

    Returns:
      True if the value had changed, otherwise False.  The first call
      always returns True.
    """
    if value == self.last_value:
      return False
    else:
      self.last_value = value
      self.var.set(self.format % value)
      return True


class SharedDataAccess(object):
  """A utility class for reading data from the memory-mapped binary
  counters file."""

  def __init__(self, data):
    """Create a new instance.

    Args:
      data: A handle to the memory-mapped file, as returned by mmap.mmap.
    """
    self.data = data

  def ByteAt(self, index):
    """Return the (unsigned) byte at the specified byte index."""
    return ord(self.CharAt(index))

  def IntAt(self, index):
    """Return the little-endian 32-byte int at the specified byte index."""
    word_str = self.data[index:index+4]
    result, = struct.unpack("I", word_str)
    return result

  def CharAt(self, index):
    """Return the ascii character at the specified byte index."""
    return self.data[index]


class Counter(object):
  """A pointer to a single counter within a binary counters file."""

  def __init__(self, data, offset):
    """Create a new instance.

    Args:
      data: the shared data access object containing the counter
      offset: the byte offset of the start of this counter
    """
    self.data = data
    self.offset = offset

  def Value(self):
    """Return the integer value of this counter."""
    return self.data.IntAt(self.offset)

  def Name(self):
    """Return the ascii name of this counter."""
    result = ""
    index = self.offset + 4
    current = self.data.ByteAt(index)
    while current:
      result += chr(current)
      index += 1
      current = self.data.ByteAt(index)
    return result


class CounterCollection(object):
  """An overlay over a counters file that provides access to the
  individual counters contained in the file."""

  def __init__(self, data):
    """Create a new instance.

    Args:
      data: the shared data access object
    """
    self.data = data
    self.max_counters = data.IntAt(4)
    self.max_name_size = data.IntAt(8)

  def CountersInUse(self):
    """Return the number of counters in active use."""
    return self.data.IntAt(12)

  def Counter(self, index):
    """Return the index'th counter."""
    return Counter(self.data, 16 + index * self.CounterSize())

  def CounterSize(self):
    """Return the size of a single counter."""
    return 4 + self.max_name_size


class ChromeCounter(object):
  """A pointer to a single counter within a binary counters file."""

  def __init__(self, data, name_offset, value_offset):
    """Create a new instance.

    Args:
      data: the shared data access object containing the counter
      name_offset: the byte offset of the start of this counter's name
      value_offset: the byte offset of the start of this counter's value
    """
    self.data = data
    self.name_offset = name_offset
    self.value_offset = value_offset

  def Value(self):
    """Return the integer value of this counter."""
    return self.data.IntAt(self.value_offset)

  def Name(self):
    """Return the ascii name of this counter."""
    result = ""
    index = self.name_offset
    current = self.data.ByteAt(index)
    while current:
      result += chr(current)
      index += 1
      current = self.data.ByteAt(index)
    return result


class ChromeCounterCollection(object):
  """An overlay over a counters file that provides access to the
  individual counters contained in the file."""

  _HEADER_SIZE = 4 * 4
  _COUNTER_NAME_SIZE = 64
  _THREAD_NAME_SIZE = 32

  def __init__(self, data):
    """Create a new instance.

    Args:
      data: the shared data access object
    """
    self.data = data
    self.max_counters = data.IntAt(8)
    self.max_threads = data.IntAt(12)
    self.counter_names_offset = \
        self._HEADER_SIZE + self.max_threads * (self._THREAD_NAME_SIZE + 2 * 4)
    self.counter_values_offset = \
        self.counter_names_offset + self.max_counters * self._COUNTER_NAME_SIZE

  def CountersInUse(self):
    """Return the number of counters in active use."""
    for i in xrange(self.max_counters):
      name_offset = self.counter_names_offset + i * self._COUNTER_NAME_SIZE
      if self.data.ByteAt(name_offset) == 0:
        return i
    return self.max_counters

  def Counter(self, i):
    """Return the i'th counter."""
    name_offset = self.counter_names_offset + i * self._COUNTER_NAME_SIZE
    value_offset = self.counter_values_offset + i * self.max_threads * 4
    return ChromeCounter(self.data, name_offset, value_offset)


def Main(data_file, name_filter):
  """Run the stats counter.

  Args:
    data_file: The counters file to monitor.
    name_filter: The regexp filter to apply to counter names.
  """
  StatsViewer(data_file, name_filter).Run()


if __name__ == "__main__":
  parser = optparse.OptionParser("usage: %prog [--filter=re] "
                                 "<stats data>|<test_shell pid>")
  parser.add_option("--filter",
                    default=".*",
                    help=("regexp filter for counter names "
                          "[default: %default]"))
  (options, args) = parser.parse_args()
  if len(args) != 1:
    parser.print_help()
    sys.exit(1)
  Main(args[0], re.compile(options.filter))
