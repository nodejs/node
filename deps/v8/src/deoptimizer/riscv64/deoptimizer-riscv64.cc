// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer/deoptimizer.h"

namespace v8 {
namespace internal {

const bool Deoptimizer::kSupportsFixedDeoptExitSizes = true;
const int Deoptimizer::kNonLazyDeoptExitSize = 5 * kInstrSize;
const int Deoptimizer::kLazyDeoptExitSize = 5 * kInstrSize;
const int Deoptimizer::kEagerWithResumeBeforeArgsSize = 6 * kInstrSize;
const int Deoptimizer::kEagerWithResumeDeoptExitSize =
    kEagerWithResumeBeforeArgsSize + 4 * kInstrSize;
const int Deoptimizer::kEagerWithResumeImmedArgs1PcOffset = kInstrSize;
const int Deoptimizer::kEagerWithResumeImmedArgs2PcOffset =
    kInstrSize + kSystemPointerSize;

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  return Float32::FromBits(
      static_cast<uint32_t>(double_registers_[n].get_bits()));
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
