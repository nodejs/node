// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/globals.h"
#include "src/isolate.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {

void Builtins::Generate_CallFunction_ReceiverIsNullOrUndefined(
    MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kNullOrUndefined,
                        TailCallMode::kDisallow);
}

void Builtins::Generate_CallFunction_ReceiverIsNotNullOrUndefined(
    MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kNotNullOrUndefined,
                        TailCallMode::kDisallow);
}

void Builtins::Generate_CallFunction_ReceiverIsAny(MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kAny,
                        TailCallMode::kDisallow);
}

void Builtins::Generate_TailCallFunction_ReceiverIsNullOrUndefined(
    MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kNullOrUndefined,
                        TailCallMode::kAllow);
}

void Builtins::Generate_TailCallFunction_ReceiverIsNotNullOrUndefined(
    MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kNotNullOrUndefined,
                        TailCallMode::kAllow);
}

void Builtins::Generate_TailCallFunction_ReceiverIsAny(MacroAssembler* masm) {
  Generate_CallFunction(masm, ConvertReceiverMode::kAny, TailCallMode::kAllow);
}

void Builtins::Generate_CallBoundFunction(MacroAssembler* masm) {
  Generate_CallBoundFunctionImpl(masm, TailCallMode::kDisallow);
}

void Builtins::Generate_TailCallBoundFunction(MacroAssembler* masm) {
  Generate_CallBoundFunctionImpl(masm, TailCallMode::kAllow);
}

void Builtins::Generate_Call_ReceiverIsNullOrUndefined(MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kNullOrUndefined,
                TailCallMode::kDisallow);
}

void Builtins::Generate_Call_ReceiverIsNotNullOrUndefined(
    MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kNotNullOrUndefined,
                TailCallMode::kDisallow);
}

void Builtins::Generate_Call_ReceiverIsAny(MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kAny, TailCallMode::kDisallow);
}

void Builtins::Generate_TailCall_ReceiverIsNullOrUndefined(
    MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kNullOrUndefined,
                TailCallMode::kAllow);
}

void Builtins::Generate_TailCall_ReceiverIsNotNullOrUndefined(
    MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kNotNullOrUndefined,
                TailCallMode::kAllow);
}

void Builtins::Generate_TailCall_ReceiverIsAny(MacroAssembler* masm) {
  Generate_Call(masm, ConvertReceiverMode::kAny, TailCallMode::kAllow);
}

void Builtins::Generate_CallForwardVarargs(MacroAssembler* masm) {
  Generate_ForwardVarargs(masm, masm->isolate()->builtins()->Call());
}

void Builtins::Generate_CallFunctionForwardVarargs(MacroAssembler* masm) {
  Generate_ForwardVarargs(masm, masm->isolate()->builtins()->CallFunction());
}

}  // namespace internal
}  // namespace v8
