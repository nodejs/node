// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_LITHIUM_ALLOCATOR_INL_H_
#define V8_CRANKSHAFT_LITHIUM_ALLOCATOR_INL_H_

#include "src/crankshaft/lithium-allocator.h"

#if V8_TARGET_ARCH_IA32
#include "src/crankshaft/ia32/lithium-ia32.h" // NOLINT
#elif V8_TARGET_ARCH_X64
#include "src/crankshaft/x64/lithium-x64.h" // NOLINT
#elif V8_TARGET_ARCH_ARM64
#include "src/crankshaft/arm64/lithium-arm64.h" // NOLINT
#elif V8_TARGET_ARCH_ARM
#include "src/crankshaft/arm/lithium-arm.h" // NOLINT
#elif V8_TARGET_ARCH_PPC
#include "src/crankshaft/ppc/lithium-ppc.h"  // NOLINT
#elif V8_TARGET_ARCH_MIPS
#include "src/crankshaft/mips/lithium-mips.h" // NOLINT
#elif V8_TARGET_ARCH_MIPS64
#include "src/crankshaft/mips64/lithium-mips64.h" // NOLINT
#elif V8_TARGET_ARCH_S390
#include "src/crankshaft/s390/lithium-s390.h"  // NOLINT
#elif V8_TARGET_ARCH_X87
#include "src/crankshaft/x87/lithium-x87.h" // NOLINT
#else
#error "Unknown architecture."
#endif

namespace v8 {
namespace internal {

bool LAllocator::IsGapAt(int index) { return chunk_->IsGapAt(index); }


LInstruction* LAllocator::InstructionAt(int index) {
  return chunk_->instructions()->at(index);
}


LGap* LAllocator::GapAt(int index) {
  return chunk_->GetGapAt(index);
}


void LAllocator::SetLiveRangeAssignedRegister(LiveRange* range, int reg) {
  if (range->Kind() == DOUBLE_REGISTERS) {
    assigned_double_registers_->Add(reg);
  } else {
    DCHECK(range->Kind() == GENERAL_REGISTERS);
    assigned_registers_->Add(reg);
  }
  range->set_assigned_register(reg, chunk()->zone());
}


}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_LITHIUM_ALLOCATOR_INL_H_
