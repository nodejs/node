// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api.h"
#include "src/deoptimizer/deoptimizer.h"
#include "src/execution/pointer-authentication.h"

namespace v8 {
namespace internal {

const bool Deoptimizer::kSupportsFixedDeoptExitSizes = true;
const int Deoptimizer::kNonLazyDeoptExitSize = kInstrSize;
#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
const int Deoptimizer::kLazyDeoptExitSize = 2 * kInstrSize;
#else
const int Deoptimizer::kLazyDeoptExitSize = 1 * kInstrSize;
#endif
const int Deoptimizer::kEagerWithResumeBeforeArgsSize = 2 * kInstrSize;
const int Deoptimizer::kEagerWithResumeDeoptExitSize =
    kEagerWithResumeBeforeArgsSize + 2 * kSystemPointerSize;
const int Deoptimizer::kEagerWithResumeImmedArgs1PcOffset = kInstrSize;
const int Deoptimizer::kEagerWithResumeImmedArgs2PcOffset =
    kInstrSize + kSystemPointerSize;

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  return Float32::FromBits(
      static_cast<uint32_t>(double_registers_[n].get_bits()));
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  Address new_context =
      static_cast<Address>(GetTop()) + offset + kPCOnStackSize;
  value = PointerAuthentication::SignAndCheckPC(value, new_context);
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerFp(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
}

void FrameDescription::SetCallerConstantPool(unsigned offset, intptr_t value) {
  // No embedded constant pool support.
  UNREACHABLE();
}

void FrameDescription::SetPc(intptr_t pc) {
  if (ENABLE_CONTROL_FLOW_INTEGRITY_BOOL) {
    CHECK(
        Deoptimizer::IsValidReturnAddress(PointerAuthentication::StripPAC(pc)));
  }
  pc_ = pc;
}

}  // namespace internal
}  // namespace v8
