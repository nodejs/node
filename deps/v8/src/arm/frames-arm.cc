// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "v8.h"

#if V8_TARGET_ARCH_ARM

#include "assembler.h"
#include "assembler-arm.h"
#include "assembler-arm-inl.h"
#include "frames.h"
#include "macro-assembler.h"
#include "macro-assembler-arm.h"

namespace v8 {
namespace internal {


Register JavaScriptFrame::fp_register() { return v8::internal::fp; }
Register JavaScriptFrame::context_register() { return cp; }
Register JavaScriptFrame::constant_pool_pointer_register() {
  ASSERT(FLAG_enable_ool_constant_pool);
  return pp;
}


Register StubFailureTrampolineFrame::fp_register() { return v8::internal::fp; }
Register StubFailureTrampolineFrame::context_register() { return cp; }
Register StubFailureTrampolineFrame::constant_pool_pointer_register() {
  ASSERT(FLAG_enable_ool_constant_pool);
  return pp;
}


Object*& ExitFrame::constant_pool_slot() const {
  ASSERT(FLAG_enable_ool_constant_pool);
  const int offset = ExitFrameConstants::kConstantPoolOffset;
  return Memory::Object_at(fp() + offset);
}


} }  // namespace v8::internal

#endif  // V8_TARGET_ARCH_ARM
