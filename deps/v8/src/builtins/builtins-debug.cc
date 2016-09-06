// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"
#include "src/debug/debug.h"

namespace v8 {
namespace internal {

void Builtins::Generate_Return_DebugBreak(MacroAssembler* masm) {
  DebugCodegen::GenerateDebugBreakStub(masm,
                                       DebugCodegen::SAVE_RESULT_REGISTER);
}

void Builtins::Generate_Slot_DebugBreak(MacroAssembler* masm) {
  DebugCodegen::GenerateDebugBreakStub(masm,
                                       DebugCodegen::IGNORE_RESULT_REGISTER);
}

void Builtins::Generate_FrameDropper_LiveEdit(MacroAssembler* masm) {
  DebugCodegen::GenerateFrameDropperLiveEdit(masm);
}

}  // namespace internal
}  // namespace v8
