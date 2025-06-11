// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/flush-instruction-cache.h"
#include "src/codegen/macro-assembler.h"
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

const int Deoptimizer::kAdaptShadowStackOffsetToSubtract = 0;

// static
void Deoptimizer::PatchToJump(Address pc, Address new_pc) {
  // Give enough space not to try to grow the buffer.
  constexpr int kSize = 64;

  Assembler masm(
      AssemblerOptions{},
      ExternalAssemblerBuffer(reinterpret_cast<uint8_t*>(pc), kSize));
  masm.mov(ip, Operand(new_pc));
  masm.mtctr(ip);
  masm.bctr();
  FlushInstructionCache(pc, kSize);
}

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  double double_val = base::ReadUnalignedValue<Float64>(
                          reinterpret_cast<Address>(simd128_registers_ + n))
                          .get_scalar();
  float float_val = static_cast<float>(double_val);
  return Float32::FromBits(base::bit_cast<uint32_t>(float_val));
}

Float64 RegisterValues::GetDoubleRegister(unsigned n) const {
  return base::ReadUnalignedValue<Float64>(
      reinterpret_cast<Address>(simd128_registers_ + n));
}

void RegisterValues::SetDoubleRegister(unsigned n, Float64 value) {
  base::WriteUnalignedValue<Float64>(
      reinterpret_cast<Address>(simd128_registers_ + n), value);
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
