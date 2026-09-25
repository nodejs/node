// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/common/code-memory-access-inl.h"
#include "src/deoptimizer/deoptimizer.h"

namespace v8 {
namespace internal {

const int Deoptimizer::kEagerDeoptExitSize = 1 * kInstrSize;
const int Deoptimizer::kLazyDeoptExitSize = 1 * kInstrSize;

const int Deoptimizer::kAdaptShadowStackOffsetToSubtract = 0;

// static
void Deoptimizer::ZapCode(Address start, Address end, RelocIterator& it) {
  // TODO(422364570): Support this platform.
}

// static
void Deoptimizer::PatchToJump(Address pc, Address new_pc) {
  RwxMemoryWriteScope rwx_write_scope("Patch jump to deopt trampoline");
  intptr_t offset = (new_pc - pc);
  // We'll overwrite only one instruction of 4-bytes. Give enough
  // space not to try to grow the buffer.
  constexpr int kSize = 128;
  AccountingAllocator allocator;
  Assembler masm(
      &allocator, AssemblerOptions{},
      ExternalAssemblerBuffer(reinterpret_cast<uint8_t*>(pc), kSize));
  masm.j(static_cast<int>(offset));
  FlushInstructionCache(pc, kSize);
}

Float32 RegisterValues::GetFloatRegister(unsigned n) const {
  return Float32::FromBits(
      static_cast<uint32_t>(double_registers_[n].get_bits()));
}
Float64 RegisterValues::GetDoubleRegister(unsigned n) const {
  return Float64::FromBits(
      static_cast<uint64_t>(double_registers_[n].get_bits()));
}

void RegisterValues::SetDoubleRegister(unsigned n, Float64 value) {
  base::WriteUnalignedValue<Float64>(
      reinterpret_cast<Address>(double_registers_ + n), value);
}

void FrameDescription::SetCallerPc(unsigned offset, intptr_t value) {
  SetFrameSlot(offset, value);
  caller_pc_ = value;
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
