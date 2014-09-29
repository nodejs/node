// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "src/v8.h"

#if V8_TARGET_ARCH_MIPS64

#include "src/assembler.h"
#include "src/frames.h"
#include "src/mips64/assembler-mips64-inl.h"
#include "src/mips64/assembler-mips64.h"

namespace v8 {
namespace internal {


Register JavaScriptFrame::fp_register() { return v8::internal::fp; }
Register JavaScriptFrame::context_register() { return cp; }
Register JavaScriptFrame::constant_pool_pointer_register() {
  UNREACHABLE();
  return no_reg;
}


Register StubFailureTrampolineFrame::fp_register() { return v8::internal::fp; }
Register StubFailureTrampolineFrame::context_register() { return cp; }
Register StubFailureTrampolineFrame::constant_pool_pointer_register() {
  UNREACHABLE();
  return no_reg;
}


Object*& ExitFrame::constant_pool_slot() const {
  UNREACHABLE();
  return Memory::Object_at(NULL);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_MIPS64
