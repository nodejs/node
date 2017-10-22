// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/frame-constants.h"

#if V8_TARGET_ARCH_ARM64

#include "src/arm64/assembler-arm64-inl.h"
#include "src/arm64/assembler-arm64.h"
#include "src/assembler.h"

#include "src/arm64/frame-constants-arm64.h"

namespace v8 {
namespace internal {

Register JavaScriptFrame::fp_register() { return v8::internal::fp; }
Register JavaScriptFrame::context_register() { return cp; }
Register JavaScriptFrame::constant_pool_pointer_register() { UNREACHABLE(); }

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_ARM64
