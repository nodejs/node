// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include <memory>

#include "src/codegen.h"
#include "src/macro-assembler.h"
#include "src/ppc/simulator-ppc.h"

namespace v8 {
namespace internal {

#define __ masm.

UnaryMathFunction CreateSqrtFunction() {
#if defined(USE_SIMULATOR)
  return nullptr;
#else
  v8::PageAllocator* page_allocator = GetPlatformPageAllocator();
  size_t allocated = 0;
  byte* buffer = AllocatePage(page_allocator,
                              page_allocator->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(AssemblerOptions{}, buffer, static_cast<int>(allocated));

  // Called from C
  __ function_descriptor();

  __ MovFromFloatParameter(d1);
  __ fsqrt(d1, d1);
  __ MovToFloatResult(d1);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(nullptr, &desc);
  DCHECK(ABI_USES_FUNCTION_DESCRIPTORS ||
         !RelocInfo::RequiresRelocationAfterCodegen(desc));

  Assembler::FlushICache(buffer, allocated);
  CHECK(SetPermissions(page_allocator, buffer, allocated,
                       PageAllocator::kReadExecute));
  return FUNCTION_CAST<UnaryMathFunction>(buffer);
#endif
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
