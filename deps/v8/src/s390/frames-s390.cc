// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/frames.h"
#include "src/assembler.h"
#include "src/macro-assembler.h"
#include "src/s390/assembler-s390-inl.h"
#include "src/s390/assembler-s390.h"
#include "src/s390/frames-s390.h"
#include "src/s390/macro-assembler-s390.h"

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

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
