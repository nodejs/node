// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/debug/debug.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

void Builtins::Generate_FrameDropperTrampoline(MacroAssembler* masm) {
  DebugCodegen::GenerateFrameDropperTrampoline(masm);
}

void Builtins::Generate_HandleDebuggerStatement(MacroAssembler* masm) {
  DebugCodegen::GenerateHandleDebuggerStatement(masm);
}

}  // namespace internal
}  // namespace v8
