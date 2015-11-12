// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_X64

#include "src/assembler.h"
#include "src/frames.h"
#include "src/x64/assembler-x64-inl.h"
#include "src/x64/assembler-x64.h"
#include "src/x64/frames-x64.h"

namespace v8 {
namespace internal {


Register JavaScriptFrame::fp_register() { return rbp; }
Register JavaScriptFrame::context_register() { return rsi; }
Register JavaScriptFrame::constant_pool_pointer_register() {
  UNREACHABLE();
  return no_reg;
}


Register StubFailureTrampolineFrame::fp_register() { return rbp; }
Register StubFailureTrampolineFrame::context_register() { return rsi; }
Register StubFailureTrampolineFrame::constant_pool_pointer_register() {
  UNREACHABLE();
  return no_reg;
}


}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_X64
