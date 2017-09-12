// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DEBUG_OBJECTS_INL_H_
#define V8_OBJECTS_DEBUG_OBJECTS_INL_H_

#include "src/objects/debug-objects.h"

#include "src/heap/heap-inl.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

CAST_ACCESSOR(BreakPointInfo)
CAST_ACCESSOR(DebugInfo)
CAST_ACCESSOR(CoverageInfo)

SMI_ACCESSORS(DebugInfo, flags, kFlagsOffset)
ACCESSORS(DebugInfo, shared, SharedFunctionInfo, kSharedFunctionInfoOffset)
SMI_ACCESSORS(DebugInfo, debugger_hints, kDebuggerHintsOffset)
ACCESSORS(DebugInfo, debug_bytecode_array, Object, kDebugBytecodeArrayOffset)
ACCESSORS(DebugInfo, break_points, FixedArray, kBreakPointsStateOffset)
ACCESSORS(DebugInfo, coverage_info, Object, kCoverageInfoOffset)

SMI_ACCESSORS(BreakPointInfo, source_position, kSourcePositionOffset)
ACCESSORS(BreakPointInfo, break_point_objects, Object, kBreakPointObjectsOffset)

bool DebugInfo::HasDebugBytecodeArray() {
  return debug_bytecode_array()->IsBytecodeArray();
}

bool DebugInfo::HasDebugCode() {
  Code* code = shared()->code();
  bool has = code->kind() == Code::FUNCTION;
  DCHECK(!has || code->has_debug_break_slots());
  return has;
}

BytecodeArray* DebugInfo::OriginalBytecodeArray() {
  DCHECK(HasDebugBytecodeArray());
  return shared()->bytecode_array();
}

BytecodeArray* DebugInfo::DebugBytecodeArray() {
  DCHECK(HasDebugBytecodeArray());
  return BytecodeArray::cast(debug_bytecode_array());
}

Code* DebugInfo::DebugCode() {
  DCHECK(HasDebugCode());
  return shared()->code();
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEBUG_OBJECTS_INL_H_
