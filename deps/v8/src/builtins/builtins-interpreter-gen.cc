// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/globals.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

void Builtins::Generate_InterpreterPushArgsThenCall(MacroAssembler* masm) {
  return Generate_InterpreterPushArgsThenCallImpl(
      masm, ConvertReceiverMode::kAny, InterpreterPushArgsMode::kOther);
}

void Builtins::Generate_InterpreterPushUndefinedAndArgsThenCall(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsThenCallImpl(
      masm, ConvertReceiverMode::kNullOrUndefined,
      InterpreterPushArgsMode::kOther);
}

void Builtins::Generate_InterpreterPushArgsThenCallWithFinalSpread(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsThenCallImpl(
      masm, ConvertReceiverMode::kAny,
      InterpreterPushArgsMode::kWithFinalSpread);
}

void Builtins::Generate_InterpreterPushArgsThenConstruct(MacroAssembler* masm) {
  return Generate_InterpreterPushArgsThenConstructImpl(
      masm, InterpreterPushArgsMode::kOther);
}

void Builtins::Generate_InterpreterPushArgsThenConstructWithFinalSpread(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsThenConstructImpl(
      masm, InterpreterPushArgsMode::kWithFinalSpread);
}

void Builtins::Generate_InterpreterPushArgsThenConstructArrayFunction(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsThenConstructImpl(
      masm, InterpreterPushArgsMode::kArrayFunction);
}

}  // namespace internal
}  // namespace v8
