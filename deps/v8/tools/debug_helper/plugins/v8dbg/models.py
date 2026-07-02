# Copyright 2026 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Python mirrors of the v8_debug_helper C ABI structs, which outlive the C results."""

import dataclasses


@dataclasses.dataclass
class HeapHints:
  """Python mirror of HeapAddresses."""
  map_space_first_page: int = 0
  old_space_first_page: int = 0
  read_only_space_first_page: int = 0
  any_heap_pointer: int = 0
  metadata_pointer_table: int = 0
  isolate_heap_member_offset: int = 0


@dataclasses.dataclass
class StructFieldSummary:
  """Python mirror of StructProperty (packed field of a Torque-struct property)."""
  name: str
  type: str
  offset: int
  num_bits: int
  shift_bits: int


@dataclasses.dataclass
class PropertySummary:
  """Python mirror of ObjectProperty (property of an inspected object)."""
  name: str
  type: str
  address: int
  kind: int
  size: int
  num_values: int
  struct_fields: list[StructFieldSummary]
  is_tagged: bool


@dataclasses.dataclass
class InspectResult:
  """Python mirror of ObjectPropertiesResult."""
  brief: str
  type: str
  type_check_result: int
  properties: list[PropertySummary]
  guessed_types: list[str]
  address: int
  # Decompressed address parsed from `brief`. Zero if the brief has none.
  display_address: int = 0
