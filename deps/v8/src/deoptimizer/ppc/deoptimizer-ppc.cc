// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/isolate-data.h"

namespace v8 {
namespace internal {

// The deopt exit sizes below depend on the following IsolateData layout
// guarantees:
#define ASSERT_OFFSET(BuiltinName)                                       \
  static_assert(IsolateData::builtin_tier0_entry_table_offset() +        \
                    Builtins::ToInt(BuiltinName) * kSystemPointerSize <= \
                0x1000)
ASSERT_OFFSET(Builtin::kDeoptimizationEntry_Eager);
ASSERT_OFFSET(Builtin::kDeoptimizationEntry_Lazy);
#undef ASSERT_OFFSET

const int Deoptimizer::kEagerDeoptExitSize = 3 * kInstrSize;
const int Deoptimizer::kLazyDeoptExitSize = 3 * kInstrSize;

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  float float_val = static_cast<float>(double_registers_[n].get_scalar());
  return Float32::FromBits(base::bit_cast<uint32_t>(float_val));
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  DCHECK(V8_EMBEDDED_CONSTANT_POOL_BOOL);
  SetFrameSlot(offset, value);
}

void FrameDescription::SetPc(intptr_t pc) { pc_ = pc; }

}  // namespace internal
}  // namespace v8
