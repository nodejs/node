// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_IA32

#include "src/codegen/flush-instruction-cache.h"
#include "src/codegen/macro-assembler.h"
#include "src/deoptimizer/deoptimizer.h"

namespace v8 {
namespace internal {

const int Deoptimizer::kEagerDeoptExitSize = 5;
const int Deoptimizer::kLazyDeoptExitSize = 5;

const int Deoptimizer::kAdaptShadowStackOffsetToSubtract = 0;

// static
void Deoptimizer::PatchToJump(Address pc, Address new_pc) {
  // We'll overwrite only one instruction of 5-bytes. Give enough
  // space not to try to grow the buffer.
  constexpr int kSize = 64;
  Assembler masm(
      AssemblerOptions{},
      ExternalAssemblerBuffer(reinterpret_cast<uint8_t*>(pc), kSize));
  masm.jmp_rel(new_pc - pc);
  FlushInstructionCache(pc, kSize);
}

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  return base::ReadUnalignedValue<Float32>(
      reinterpret_cast<Address>(simd128_registers_ + n));
}

Float64 RegisterValues::GetDoubleRegister(unsigned n) const {
  V8_ASSUME(n < arraysize(simd128_registers_));
  return base::ReadUnalignedValue<Float64>(
      reinterpret_cast<Address>(simd128_registers_ + n));
}

void RegisterValues::SetDoubleRegister(unsigned n, Float64 value) {
  V8_ASSUME(n < arraysize(simd128_registers_));
  base::WriteUnalignedValue(reinterpret_cast<Address>(simd128_registers_ + n),
                            value);
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

#endif  // V8_TARGET_ARCH_IA32
