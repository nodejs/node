# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Resolution of the current Isolate and HeapHints through a debugger-specific
resolver.
"""

import os
import traceback

from .models import HeapHints

_VERBOSE = os.environ.get("V8_DEBUG_HELPER_VERBOSE", "") != ""

_ISOLATE_SYMBOL = "v8::internal::g_current_isolate_"


def resolve_current_isolate(resolver):
  """Resolve the current Isolate of the selected thread.

  Returns its address, 0 if the thread has no current isolate, or None if
  the TLS slot cannot be resolved or read.
  """
  # TODO(joyee): cache this per stop and thread selected.
  sym_addr = resolver.symbol_address(_ISOLATE_SYMBOL)
  if not sym_addr:
    return None
  return resolver.read_pointer(sym_addr)


def resolve_heap_hints(resolver):
  """Resolve V8 isolate symbols and offsets through `resolver` into HeapHints.

  Only uses debug info, which the debugger resolves cheaply, since this runs
  on every inspection. The hints only improve inspection and are not
  required for it, so resolution failures degrade to empty hints.
  """
  try:
    return _resolve_heap_hints(resolver)
  except Exception:
    if _VERBOSE:
      traceback.print_exc()
    return HeapHints()


def _resolve_heap_hints(resolver):
  hints = HeapHints()
  sym_addr = resolver.debug_symbol_address(_ISOLATE_SYMBOL)
  isolate_addr = resolver.read_pointer(sym_addr) if sym_addr else None
  if not isolate_addr:
    return hints

  # Read metadata_pointer_table_ from the IsolateGroup of the current Isolate.
  group_offset = resolver.field_offset("v8::internal::Isolate",
                                       "isolate_group_")
  if group_offset is not None:
    group_ptr = resolver.read_pointer(isolate_addr + group_offset)
    mpt_offset = resolver.field_offset("v8::internal::IsolateGroup",
                                       "metadata_pointer_table_")
    if group_ptr and mpt_offset is not None:
      hints.metadata_pointer_table = group_ptr + mpt_offset

  # Get the isolate Isolate::heap_ member offset.
  heap_isolate_offset = resolver.field_offset("v8::internal::Heap", "isolate_")
  if heap_isolate_offset is not None:
    hints.isolate_heap_member_offset = int(heap_isolate_offset)

  # TODO(joyee): fill the *_space_first_page hints from the resolved isolate so
  # FindKnownObject()/FindKnownMapInstanceTypes() return better results.

  # any_heap_pointer is filled in by `_run_inspect` from the CLI address.
  return hints
