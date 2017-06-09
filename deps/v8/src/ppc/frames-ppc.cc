// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_PPC

#include "src/assembler.h"
#include "src/frames.h"
#include "src/macro-assembler.h"

#include "src/ppc/assembler-ppc.h"
#include "src/ppc/assembler-ppc-inl.h"
#include "src/ppc/frames-ppc.h"
#include "src/ppc/macro-assembler-ppc.h"

namespace v8 {
namespace internal {


Register JavaScriptFrame::fp_register() { return v8::internal::fp; }
Register JavaScriptFrame::context_register() { return cp; }
Register JavaScriptFrame::constant_pool_pointer_register() {
  DCHECK(FLAG_enable_embedded_constant_pool);
  return kConstantPoolRegister;
}


Register StubFailureTrampolineFrame::fp_register() { return v8::internal::fp; }
Register StubFailureTrampolineFrame::context_register() { return cp; }
Register StubFailureTrampolineFrame::constant_pool_pointer_register() {
  DCHECK(FLAG_enable_embedded_constant_pool);
  return kConstantPoolRegister;
}
}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_PPC
