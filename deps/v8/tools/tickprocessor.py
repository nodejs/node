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

import csv, splaytree, sys, re
from operator import itemgetter
import getopt, os, string

class CodeEntry(object):

  def __init__(self, start_addr, name):
    self.start_addr = start_addr
    self.tick_count = 0
    self.name = name
    self.stacks = {}

  def Tick(self, pc, stack):
    self.tick_count += 1
    if len(stack) > 0:
      stack.insert(0, self.ToString())
      stack_key = tuple(stack)
      self.stacks[stack_key] = self.stacks.setdefault(stack_key, 0) + 1

  def RegionTicks(self):
    return None

  def SetStartAddress(self, start_addr):
    self.start_addr = start_addr

  def ToString(self):
    return self.name

  def IsSharedLibraryEntry(self):
    return False

  def IsICEntry(self):
    return False

  def IsJSFunction(self):
    return False

class SharedLibraryEntry(CodeEntry):

  def __init__(self, start_addr, name):
    CodeEntry.__init__(self, start_addr, name)

  def IsSharedLibraryEntry(self):
    return True


class JSCodeEntry(CodeEntry):

  def __init__(self, start_addr, name, type, size, assembler):
    CodeEntry.__init__(self, start_addr, name)
    self.type = type
    self.size = size
    self.assembler = assembler
    self.region_ticks = None
    self.builtin_ic_re = re.compile('^(Keyed)?(Call|Load|Store)IC_')

  def Tick(self, pc, stack):
    super(JSCodeEntry, self).Tick(pc, stack)
    if not pc is None:
      offset = pc - self.start_addr
      seen = []
      narrowest = None
      narrowest_width = None
      for region in self.Regions():
        if region.Contains(offset):
          if (not region.name in seen):
            seen.append(region.name)
          if narrowest is None or region.Width() < narrowest.Width():
            narrowest = region
      if len(seen) == 0:
        return
      if self.region_ticks is None:
        self.region_ticks = {}
      for name in seen:
        if not name in self.region_ticks:
          self.region_ticks[name] = [0, 0]
        self.region_ticks[name][0] += 1
        if name == narrowest.name:
          self.region_ticks[name][1] += 1

  def RegionTicks(self):
    return self.region_ticks

  def Regions(self):
    if self.assembler:
      return self.assembler.regions
    else:
      return []

  def ToString(self):
    name = self.name
    if name == '':
      name = '<anonymous>'
    elif name.startswith(' '):
      name = '<anonymous>' + name
    return self.type + ': ' + name

  def IsICEntry(self):
    return self.type in ('CallIC', 'LoadIC', 'StoreIC') or \
      (self.type == 'Builtin' and self.builtin_ic_re.match(self.name))

  def IsJSFunction(self):
    return self.type in ('Function', 'LazyCompile', 'Script')

class CodeRegion(object):

  def __init__(self, start_offset, name):
    self.start_offset = start_offset
    self.name = name
    self.end_offset = None

  def Contains(self, pc):
    return (self.start_offset <= pc) and (pc <= self.end_offset)

  def Width(self):
    return self.end_offset - self.start_offset


class Assembler(object):

  def __init__(self):
    # Mapping from region ids to open regions
    self.pending_regions = {}
    self.regions = []


class FunctionEnumerator(object):

  def __init__(self):
    self.known_funcs = {}
    self.next_func_id = 0

  def GetFunctionId(self, name):
    if not self.known_funcs.has_key(name):
      self.known_funcs[name] = self.next_func_id
      self.next_func_id += 1
    return self.known_funcs[name]

  def GetKnownFunctions(self):
    known_funcs_items = self.known_funcs.items();
    known_funcs_items.sort(key = itemgetter(1))
    result = []
    for func, id_not_used in known_funcs_items:
      result.append(func)
    return result


VMStates = { 'JS': 0, 'GC': 1, 'COMPILER': 2, 'OTHER': 3, 'EXTERNAL' : 4 }


class TickProcessor(object):

  def __init__(self):
    self.log_file = ''
    self.deleted_code = []
    self.vm_extent = {}
    # Map from assembler ids to the pending assembler objects
    self.pending_assemblers = {}
    # Map from code addresses the have been allocated but not yet officially
    # created to their assemblers.
    self.assemblers = {}
    self.js_entries = splaytree.SplayTree()
    self.cpp_entries = splaytree.SplayTree()
    self.total_number_of_ticks = 0
    self.number_of_library_ticks = 0
    self.unaccounted_number_of_ticks = 0
    self.excluded_number_of_ticks = 0
    self.number_of_gc_ticks = 0
    # Flag indicating whether to ignore unaccounted ticks in the report
    self.ignore_unknown = False
    self.func_enum = FunctionEnumerator()
    self.packed_stacks = []

  def ProcessLogfile(self, filename, included_state = None, ignore_unknown = False, separate_ic = False, call_graph_json = False):
    self.log_file = filename
    self.included_state = included_state
    self.ignore_unknown = ignore_unknown
    self.separate_ic = separate_ic
    self.call_graph_json = call_graph_json

    try:
      logfile = open(filename, 'rb')
    except IOError:
      sys.exit("Could not open logfile: " + filename)
    try:
      try:
        logreader = csv.reader(logfile)
        row_num = 1
        for row in logreader:
          row_num += 1
          if row[0] == 'tick':
            self.ProcessTick(int(row[1], 16), int(row[2], 16), int(row[3], 16), int(row[4]), self.PreprocessStack(row[5:]))
          elif row[0] == 'code-creation':
            self.ProcessCodeCreation(row[1], int(row[2], 16), int(row[3]), row[4])
          elif row[0] == 'code-move':
            self.ProcessCodeMove(int(row[1], 16), int(row[2], 16))
          elif row[0] == 'code-delete':
            self.ProcessCodeDelete(int(row[1], 16))
          elif row[0] == 'function-creation':
            self.ProcessFunctionCreation(int(row[1], 16), int(row[2], 16))
          elif row[0] == 'function-move':
            self.ProcessFunctionMove(int(row[1], 16), int(row[2], 16))
          elif row[0] == 'function-delete':
            self.ProcessFunctionDelete(int(row[1], 16))
          elif row[0] == 'shared-library':
            self.AddSharedLibraryEntry(row[1], int(row[2], 16), int(row[3], 16))
            self.ParseVMSymbols(row[1], int(row[2], 16), int(row[3], 16))
          elif row[0] == 'begin-code-region':
            self.ProcessBeginCodeRegion(int(row[1], 16), int(row[2], 16), int(row[3], 16), row[4])
          elif row[0] == 'end-code-region':
            self.ProcessEndCodeRegion(int(row[1], 16), int(row[2], 16), int(row[3], 16))
          elif row[0] == 'code-allocate':
            self.ProcessCodeAllocate(int(row[1], 16), int(row[2], 16))
      except csv.Error:
        print("parse error in line " + str(row_num))
        raise
    finally:
      logfile.close()

  def AddSharedLibraryEntry(self, filename, start, end):
    # Mark the pages used by this library.
    i = start
    while i < end:
      page = i >> 12
      self.vm_extent[page] = 1
      i += 4096
    # Add the library to the entries so that ticks for which we do not
    # have symbol information is reported as belonging to the library.
    self.cpp_entries.Insert(start, SharedLibraryEntry(start, filename))

  def ParseVMSymbols(self, filename, start, end):
    return

  def ProcessCodeAllocate(self, addr, assem):
    if assem in self.pending_assemblers:
      assembler = self.pending_assemblers.pop(assem)
      self.assemblers[addr] = assembler

  def ProcessCodeCreation(self, type, addr, size, name):
    if addr in self.assemblers:
      assembler = self.assemblers.pop(addr)
    else:
      assembler = None
    self.js_entries.Insert(addr, JSCodeEntry(addr, name, type, size, assembler))

  def ProcessCodeMove(self, from_addr, to_addr):
    try:
      removed_node = self.js_entries.Remove(from_addr)
      removed_node.value.SetStartAddress(to_addr);
      self.js_entries.Insert(to_addr, removed_node.value)
    except splaytree.KeyNotFoundError:
      print('Code move event for unknown code: 0x%x' % from_addr)

  def ProcessCodeDelete(self, from_addr):
    try:
      removed_node = self.js_entries.Remove(from_addr)
      self.deleted_code.append(removed_node.value)
    except splaytree.KeyNotFoundError:
      print('Code delete event for unknown code: 0x%x' % from_addr)

  def ProcessFunctionCreation(self, func_addr, code_addr):
    js_entry_node = self.js_entries.Find(code_addr)
    if js_entry_node:
      js_entry = js_entry_node.value
      self.js_entries.Insert(func_addr, JSCodeEntry(func_addr, js_entry.name, js_entry.type, 1, None))

  def ProcessFunctionMove(self, from_addr, to_addr):
    try:
      removed_node = self.js_entries.Remove(from_addr)
      removed_node.value.SetStartAddress(to_addr);
      self.js_entries.Insert(to_addr, removed_node.value)
    except splaytree.KeyNotFoundError:
      return

  def ProcessFunctionDelete(self, from_addr):
    try:
      removed_node = self.js_entries.Remove(from_addr)
      self.deleted_code.append(removed_node.value)
    except splaytree.KeyNotFoundError:
      return

  def ProcessBeginCodeRegion(self, id, assm, start, name):
    if not assm in self.pending_assemblers:
      self.pending_assemblers[assm] = Assembler()
    assembler = self.pending_assemblers[assm]
    assembler.pending_regions[id] = CodeRegion(start, name)

  def ProcessEndCodeRegion(self, id, assm, end):
    assm = self.pending_assemblers[assm]
    region = assm.pending_regions.pop(id)
    region.end_offset = end
    assm.regions.append(region)

  def IncludeTick(self, pc, sp, state):
    return (self.included_state is None) or (self.included_state == state)

  def FindEntry(self, pc):
    page = pc >> 12
    if page in self.vm_extent:
      entry = self.cpp_entries.FindGreatestsLessThan(pc)
      if entry != None:
        return entry.value
      else:
        return entry
    max = self.js_entries.FindMax()
    min = self.js_entries.FindMin()
    if max != None and pc < (max.key + max.value.size) and pc > min.key:
      return self.js_entries.FindGreatestsLessThan(pc).value
    return None

  def PreprocessStack(self, stack):
    # remove all non-addresses (e.g. 'overflow') and convert to int
    result = []
    for frame in stack:
      if frame.startswith('0x'):
        result.append(int(frame, 16))
    return result

  def ProcessStack(self, stack):
    result = []
    for frame in stack:
      entry = self.FindEntry(frame)
      if entry != None:
        result.append(entry.ToString())
    return result

  def ProcessTick(self, pc, sp, func, state, stack):
    if state == VMStates['GC']:
      self.number_of_gc_ticks += 1
    if not self.IncludeTick(pc, sp, state):
      self.excluded_number_of_ticks += 1;
      return
    self.total_number_of_ticks += 1
    entry = self.FindEntry(pc)
    if entry == None:
      self.unaccounted_number_of_ticks += 1
      return
    if entry.IsSharedLibraryEntry():
      self.number_of_library_ticks += 1
    if entry.IsICEntry() and not self.separate_ic:
      if len(stack) > 0:
        caller_pc = stack.pop(0)
        self.total_number_of_ticks -= 1
        self.ProcessTick(caller_pc, sp, func, state, stack)
      else:
        self.unaccounted_number_of_ticks += 1
    else:
      processed_stack = self.ProcessStack(stack)
      if not entry.IsSharedLibraryEntry() and not entry.IsJSFunction():
        func_entry_node = self.js_entries.Find(func)
        if func_entry_node and func_entry_node.value.IsJSFunction():
          processed_stack.insert(0, func_entry_node.value.ToString())
      entry.Tick(pc, processed_stack)
      if self.call_graph_json:
        self.AddToPackedStacks(pc, stack)

  def AddToPackedStacks(self, pc, stack):
    full_stack = stack
    full_stack.insert(0, pc)
    func_names = self.ProcessStack(full_stack)
    func_ids = []
    for func in func_names:
      func_ids.append(self.func_enum.GetFunctionId(func))
    self.packed_stacks.append(func_ids)

  def PrintResults(self):
    if not self.call_graph_json:
      self.PrintStatistics()
    else:
      self.PrintCallGraphJSON()

  def PrintStatistics(self):
    print('Statistical profiling result from %s, (%d ticks, %d unaccounted, %d excluded).' %
          (self.log_file,
           self.total_number_of_ticks,
           self.unaccounted_number_of_ticks,
           self.excluded_number_of_ticks))
    if self.total_number_of_ticks > 0:
      js_entries = self.js_entries.ExportValueList()
      js_entries.extend(self.deleted_code)
      cpp_entries = self.cpp_entries.ExportValueList()
      # Print the unknown ticks percentage if they are not ignored.
      if not self.ignore_unknown and self.unaccounted_number_of_ticks > 0:
        self.PrintHeader('Unknown')
        self.PrintCounter(self.unaccounted_number_of_ticks)
      # Print the library ticks.
      self.PrintHeader('Shared libraries')
      self.PrintEntries(cpp_entries, lambda e:e.IsSharedLibraryEntry())
      # Print the JavaScript ticks.
      self.PrintHeader('JavaScript')
      self.PrintEntries(js_entries, lambda e:not e.IsSharedLibraryEntry())
      # Print the C++ ticks.
      self.PrintHeader('C++')
      self.PrintEntries(cpp_entries, lambda e:not e.IsSharedLibraryEntry())
      # Print the GC ticks.
      self.PrintHeader('GC')
      self.PrintCounter(self.number_of_gc_ticks)
      # Print call profile.
      print('\n [Call profile]:')
      print('   total  call path')
      js_entries.extend(cpp_entries)
      self.PrintCallProfile(js_entries)

  def PrintHeader(self, header_title):
    print('\n [%s]:' % header_title)
    print('   ticks  total  nonlib   name')

  def PrintCounter(self, ticks_count):
    percentage = ticks_count * 100.0 / self.total_number_of_ticks
    print('  %(ticks)5d  %(total)5.1f%%' % {
      'ticks' : ticks_count,
      'total' : percentage,
    })

  def PrintEntries(self, entries, condition):
    # If ignoring unaccounted ticks don't include these in percentage
    # calculations
    number_of_accounted_ticks = self.total_number_of_ticks
    if self.ignore_unknown:
      number_of_accounted_ticks -= self.unaccounted_number_of_ticks

    number_of_non_library_ticks = number_of_accounted_ticks - self.number_of_library_ticks
    entries.sort(key=lambda e: (e.tick_count, e.ToString()), reverse=True)
    for entry in entries:
      if entry.tick_count > 0 and condition(entry):
        total_percentage = entry.tick_count * 100.0 / number_of_accounted_ticks
        if entry.IsSharedLibraryEntry():
          non_library_percentage = 0
        else:
          non_library_percentage = entry.tick_count * 100.0 / number_of_non_library_ticks
        print('  %(ticks)5d  %(total)5.1f%% %(nonlib)6.1f%%  %(name)s' % {
          'ticks' : entry.tick_count,
          'total' : total_percentage,
          'nonlib' : non_library_percentage,
          'name' : entry.ToString()
        })
        region_ticks = entry.RegionTicks()
        if not region_ticks is None:
          items = region_ticks.items()
          items.sort(key=lambda e: e[1][1], reverse=True)
          for (name, ticks) in items:
            print('                      flat   cum')
            print('                     %(flat)5.1f%% %(accum)5.1f%% %(name)s' % {
              'flat' : ticks[1] * 100.0 / entry.tick_count,
              'accum' : ticks[0] * 100.0 / entry.tick_count,
              'name': name
            })

  def PrintCallProfile(self, entries):
    all_stacks = {}
    total_stacks = 0
    for entry in entries:
      all_stacks.update(entry.stacks)
      for count in entry.stacks.itervalues():
        total_stacks += count
    all_stacks_items = all_stacks.items();
    all_stacks_items.sort(key = itemgetter(1), reverse=True)
    missing_percentage = (self.total_number_of_ticks - total_stacks) * 100.0 / self.total_number_of_ticks
    print('  %(ticks)5d  %(total)5.1f%%  <no call path information>' % {
      'ticks' : self.total_number_of_ticks - total_stacks,
      'total' : missing_percentage
    })
    for stack, count in all_stacks_items:
      total_percentage = count * 100.0 / self.total_number_of_ticks
      print('  %(ticks)5d  %(total)5.1f%%  %(call_path)s' % {
        'ticks' : count,
        'total' : total_percentage,
        'call_path' : stack[0] + '  <-  ' + stack[1]
      })

  def PrintCallGraphJSON(self):
    print('\nvar __profile_funcs = ["' +
          '",\n"'.join(self.func_enum.GetKnownFunctions()) +
          '"];')
    print('var __profile_ticks = [')
    str_packed_stacks = []
    for stack in self.packed_stacks:
      str_packed_stacks.append('[' + ','.join(map(str, stack)) + ']')
    print(',\n'.join(str_packed_stacks))
    print('];')

class CmdLineProcessor(object):

  def __init__(self):
    self.options = ["js",
                    "gc",
                    "compiler",
                    "other",
                    "external",
                    "ignore-unknown",
                    "separate-ic",
                    "call-graph-json"]
    # default values
    self.state = None
    self.ignore_unknown = False
    self.log_file = None
    self.separate_ic = False
    self.call_graph_json = False

  def ProcessArguments(self):
    try:
      opts, args = getopt.getopt(sys.argv[1:], "jgcoe", self.options)
    except getopt.GetoptError:
      self.PrintUsageAndExit()
    for key, value in opts:
      if key in ("-j", "--js"):
        self.state = VMStates['JS']
      if key in ("-g", "--gc"):
        self.state = VMStates['GC']
      if key in ("-c", "--compiler"):
        self.state = VMStates['COMPILER']
      if key in ("-o", "--other"):
        self.state = VMStates['OTHER']
      if key in ("-e", "--external"):
        self.state = VMStates['EXTERNAL']
      if key in ("--ignore-unknown"):
        self.ignore_unknown = True
      if key in ("--separate-ic"):
        self.separate_ic = True
      if key in ("--call-graph-json"):
        self.call_graph_json = True
    self.ProcessRequiredArgs(args)

  def ProcessRequiredArgs(self, args):
    return

  def GetRequiredArgsNames(self):
    return

  def PrintUsageAndExit(self):
    print('Usage: %(script_name)s --{%(opts)s} %(req_opts)s' % {
        'script_name': os.path.basename(sys.argv[0]),
        'opts': string.join(self.options, ','),
        'req_opts': self.GetRequiredArgsNames()
    })
    sys.exit(2)

  def RunLogfileProcessing(self, tick_processor):
    tick_processor.ProcessLogfile(self.log_file, self.state, self.ignore_unknown,
                                  self.separate_ic, self.call_graph_json)


if __name__ == '__main__':
  sys.exit('You probably want to run windows-tick-processor.py or linux-tick-processor.py.')
