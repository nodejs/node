#!/usr/bin/env python
# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This program writes a C++ file that can be used to look up whether a given
address matches known object locations. The first argument is the directory
containing the file v8heapconst.py; the second argument is the output .cc file.
"""

import sys
sys.path.insert(0, sys.argv[1])
import v8heapconst

out = """
#include <cstdint>
#include <string>

#include "src/common/ptr-compr-inl.h"
#include "tools/debug_helper/debug-helper-internal.h"

namespace v8_debug_helper_internal {
"""

def iterate_objects(target_space, camel_space_name):
  global out
  result = []
  for (space, offset), (instance_type, name) in v8heapconst.KNOWN_MAPS.items():
    if space == target_space:
      result.append((offset, name))
  for (space, offset), name in v8heapconst.KNOWN_OBJECTS.items():
    if space == target_space:
      result.append((offset, name))
  out = out + '\nstd::string FindKnownObjectIn' + camel_space_name \
      + '(uintptr_t offset) {\n  switch (offset) {\n'
  for offset, name in result:
    out = out + '    case ' + str(offset) + ': return "' + name + '";\n'
  out = out + '    default: return "";\n  }\n}\n'

iterate_objects('map_space', 'MapSpace')
iterate_objects('read_only_space', 'ReadOnlySpace')
iterate_objects('old_space', 'OldSpace')

def iterate_maps(target_space, camel_space_name):
  global out
  out = out + '\nint FindKnownMapInstanceTypeIn' + camel_space_name \
      + '(uintptr_t offset) {\n  switch (offset) {\n'
  for (space, offset), (instance_type, name) in v8heapconst.KNOWN_MAPS.items():
    if space == target_space:
      out = out + '    case ' + str(offset) + ': return ' + str(instance_type) \
          + ';\n'
  out = out + '    default: return -1;\n  }\n}\n'

iterate_maps('map_space', 'MapSpace')
iterate_maps('read_only_space', 'ReadOnlySpace')

out = out + '\nvoid FillInUnknownHeapAddresses(' + \
    'd::HeapAddresses* heap_addresses, uintptr_t any_uncompressed_ptr) {\n'
if (hasattr(v8heapconst, 'HEAP_FIRST_PAGES')):  # Only exists in ptr-compr builds.
  out = out + '  if (heap_addresses->any_heap_pointer == 0) {\n'
  out = out + '    heap_addresses->any_heap_pointer = any_uncompressed_ptr;\n'
  out = out + '  }\n'
  expected_spaces = set(['map_space', 'read_only_space', 'old_space'])
  for offset, space_name in v8heapconst.HEAP_FIRST_PAGES.items():
    if (space_name in expected_spaces):
      out = out + '  if (heap_addresses->' + space_name + '_first_page == 0) {\n'
      out = out + '    heap_addresses->' + space_name + \
          '_first_page = i::DecompressTaggedPointer(any_uncompressed_ptr, ' + \
          str(offset) + ');\n'
      out = out + '  }\n'
out = out + '}\n'

out = out + '\n}\n'

try:
  with open(sys.argv[2], "r") as out_file:
    if out == out_file.read():
      sys.exit(0)  # No modification needed.
except:
  pass  # File probably doesn't exist; write it.
with open(sys.argv[2], "w") as out_file:
  out_file.write(out)
