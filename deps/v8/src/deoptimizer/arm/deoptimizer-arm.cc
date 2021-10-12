// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/isolate-data.h"

namespace v8 {
namespace internal {

// The deopt exit sizes below depend on the following IsolateData layout
// guarantees:
#define ASSERT_OFFSET(BuiltinName)                                       \
  STATIC_ASSERT(IsolateData::builtin_tier0_entry_table_offset() +        \
                    Builtins::ToInt(BuiltinName) * kSystemPointerSize <= \
                0x1000)
ASSERT_OFFSET(Builtin::kDeoptimizationEntry_Eager);
ASSERT_OFFSET(Builtin::kDeoptimizationEntry_Lazy);
ASSERT_OFFSET(Builtin::kDeoptimizationEntry_Soft);
ASSERT_OFFSET(Builtin::kDeoptimizationEntry_Bailout);
#undef ASSERT_OFFSET

const bool Deoptimizer::kSupportsFixedDeoptExitSizes = true;
const int Deoptimizer::kNonLazyDeoptExitSize = 2 * kInstrSize;
const int Deoptimizer::kLazyDeoptExitSize = 2 * kInstrSize;
const int Deoptimizer::kEagerWithResumeBeforeArgsSize = 3 * kInstrSize;
const int Deoptimizer::kEagerWithResumeDeoptExitSize =
    kEagerWithResumeBeforeArgsSize + 2 * kSystemPointerSize;
const int Deoptimizer::kEagerWithResumeImmedArgs1PcOffset = kInstrSize;
const int Deoptimizer::kEagerWithResumeImmedArgs2PcOffset =
    kInstrSize + kSystemPointerSize;

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  const int kShift = n % 2 == 0 ? 0 : 32;

  return Float32::FromBits(
      static_cast<uint32_t>(double_registers_[n / 2].get_bits() >> kShift));
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No embedded constant pool support.
  UNREACHABLE();
}

void FrameDescription::SetPc(intptr_t pc) { pc_ = pc; }

}  // namespace internal
}  // namespace v8
