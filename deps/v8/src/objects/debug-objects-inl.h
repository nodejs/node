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
CAST_ACCESSOR(BreakPoint)

SMI_ACCESSORS(DebugInfo, flags, kFlagsOffset)
ACCESSORS(DebugInfo, shared, SharedFunctionInfo, kSharedFunctionInfoOffset)
SMI_ACCESSORS(DebugInfo, debugger_hints, kDebuggerHintsOffset)
ACCESSORS(DebugInfo, debug_bytecode_array, Object, kDebugBytecodeArrayOffset)
ACCESSORS(DebugInfo, break_points, FixedArray, kBreakPointsStateOffset)
ACCESSORS(DebugInfo, coverage_info, Object, kCoverageInfoOffset)

SMI_ACCESSORS(BreakPointInfo, source_position, kSourcePositionOffset)
ACCESSORS(BreakPointInfo, break_points, Object, kBreakPointsOffset)

SMI_ACCESSORS(BreakPoint, id, kIdOffset)
ACCESSORS(BreakPoint, condition, String, kConditionOffset)

bool DebugInfo::HasDebugBytecodeArray() {
  return debug_bytecode_array()->IsBytecodeArray();
}

BytecodeArray* DebugInfo::OriginalBytecodeArray() {
  DCHECK(HasDebugBytecodeArray());
  return shared()->GetBytecodeArray();
}

BytecodeArray* DebugInfo::DebugBytecodeArray() {
  DCHECK(HasDebugBytecodeArray());
  return BytecodeArray::cast(debug_bytecode_array());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEBUG_OBJECTS_INL_H_
