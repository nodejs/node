// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_DEBUG_OBJECTS_INL_H_
#define V8_OBJECTS_DEBUG_OBJECTS_INL_H_

#include "src/objects/debug-objects.h"

#include "src/heap/heap-write-barrier-inl.h"
#include "src/objects/code-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

TQ_OBJECT_CONSTRUCTORS_IMPL(BreakPoint)
TQ_OBJECT_CONSTRUCTORS_IMPL(BreakPointInfo)
OBJECT_CONSTRUCTORS_IMPL(CoverageInfo, FixedArray)
TQ_OBJECT_CONSTRUCTORS_IMPL(DebugInfo)

NEVER_READ_ONLY_SPACE_IMPL(DebugInfo)

CAST_ACCESSOR(CoverageInfo)

TQ_SMI_ACCESSORS(DebugInfo, flags)
TQ_SMI_ACCESSORS(DebugInfo, debugger_hints)

BIT_FIELD_ACCESSORS(DebugInfo, debugger_hints, side_effect_state,
                    DebugInfo::SideEffectStateBits)
BIT_FIELD_ACCESSORS(DebugInfo, debugger_hints, debug_is_blackboxed,
                    DebugInfo::DebugIsBlackboxedBit)
BIT_FIELD_ACCESSORS(DebugInfo, debugger_hints, computed_debug_is_blackboxed,
                    DebugInfo::ComputedDebugIsBlackboxedBit)
BIT_FIELD_ACCESSORS(DebugInfo, debugger_hints, debugging_id,
                    DebugInfo::DebuggingIdBits)

TQ_SMI_ACCESSORS(BreakPointInfo, source_position)

TQ_SMI_ACCESSORS(BreakPoint, id)

bool DebugInfo::HasInstrumentedBytecodeArray() {
  DCHECK_EQ(debug_bytecode_array().IsBytecodeArray(),
            original_bytecode_array().IsBytecodeArray());
  return debug_bytecode_array().IsBytecodeArray();
}

BytecodeArray DebugInfo::OriginalBytecodeArray() {
  DCHECK(HasInstrumentedBytecodeArray());
  return BytecodeArray::cast(original_bytecode_array());
}

BytecodeArray DebugInfo::DebugBytecodeArray() {
  DCHECK(HasInstrumentedBytecodeArray());
  DCHECK_EQ(shared().GetDebugBytecodeArray(), debug_bytecode_array());
  return BytecodeArray::cast(debug_bytecode_array());
}

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_DEBUG_OBJECTS_INL_H_
