// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/deoptimizer/deoptimizer.h"

namespace v8 {
namespace internal {

const bool Deoptimizer::kSupportsFixedDeoptExitSizes = true;
const int Deoptimizer::kNonLazyDeoptExitSize = 6 + 2;
const int Deoptimizer::kLazyDeoptExitSize = 6 + 2;
const int Deoptimizer::kEagerWithResumeBeforeArgsSize = 6 + 2 + 6;
const int Deoptimizer::kEagerWithResumeDeoptExitSize =
    kEagerWithResumeBeforeArgsSize + 2 * kSystemPointerSize;
const int Deoptimizer::kEagerWithResumeImmedArgs1PcOffset = 6;
const int Deoptimizer::kEagerWithResumeImmedArgs2PcOffset =
    6 + kSystemPointerSize;

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  return Float32::FromBits(
      static_cast<uint32_t>(double_registers_[n].get_bits() >> 32));
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No out-of-line constant pool support.
  UNREACHABLE();
}

void FrameDescription::SetPc(intptr_t pc) { pc_ = pc; }

}  // namespace internal
}  // namespace v8
