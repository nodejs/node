// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include <memory>

#include "src/codegen.h"
#include "src/macro-assembler.h"
#include "src/s390/simulator-s390.h"

namespace v8 {
namespace internal {

#define __ masm.

UnaryMathFunctionWithIsolate CreateSqrtFunction(Isolate* isolate) {
#if defined(USE_SIMULATOR)
  return nullptr;
#else
  size_t allocated = 0;
  byte* buffer =
      AllocateSystemPage(isolate->heap()->GetRandomMmapAddr(), &allocated);
  if (buffer == nullptr) return nullptr;

  MacroAssembler masm(isolate, buffer, static_cast<int>(allocated),
                      CodeObjectRequired::kNo);

  __ MovFromFloatParameter(d0);
  __ sqdbr(d0, d0);
  __ MovToFloatResult(d0);
  __ Ret();

  CodeDesc desc;
  masm.GetCode(isolate, &desc);
  DCHECK(ABI_USES_FUNCTION_DESCRIPTORS ||
         !RelocInfo::RequiresRelocation(isolate, desc));

  Assembler::FlushICache(isolate, buffer, allocated);
  CHECK(base::OS::SetPermissions(buffer, allocated,
                                 base::OS::MemoryPermission::kReadExecute));
  return FUNCTION_CAST<UnaryMathFunctionWithIsolate>(buffer);
#endif
}

#undef __

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
