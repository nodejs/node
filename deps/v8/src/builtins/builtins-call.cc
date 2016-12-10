// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

namespace v8 {
namespace internal {

Handle<Code> Builtins::CallFunction(ConvertReceiverMode mode,
                                    TailCallMode tail_call_mode) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      switch (mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return CallFunction_ReceiverIsNullOrUndefined();
        case ConvertReceiverMode::kNotNullOrUndefined:
          return CallFunction_ReceiverIsNotNullOrUndefined();
        case ConvertReceiverMode::kAny:
          return CallFunction_ReceiverIsAny();
      }
      break;
    case TailCallMode::kAllow:
      switch (mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return TailCallFunction_ReceiverIsNullOrUndefined();
        case ConvertReceiverMode::kNotNullOrUndefined:
          return TailCallFunction_ReceiverIsNotNullOrUndefined();
        case ConvertReceiverMode::kAny:
          return TailCallFunction_ReceiverIsAny();
      }
      break;
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

Handle<Code> Builtins::Call(ConvertReceiverMode mode,
                            TailCallMode tail_call_mode) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      switch (mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return Call_ReceiverIsNullOrUndefined();
        case ConvertReceiverMode::kNotNullOrUndefined:
          return Call_ReceiverIsNotNullOrUndefined();
        case ConvertReceiverMode::kAny:
          return Call_ReceiverIsAny();
      }
      break;
    case TailCallMode::kAllow:
      switch (mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return TailCall_ReceiverIsNullOrUndefined();
        case ConvertReceiverMode::kNotNullOrUndefined:
          return TailCall_ReceiverIsNotNullOrUndefined();
        case ConvertReceiverMode::kAny:
          return TailCall_ReceiverIsAny();
      }
      break;
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

Handle<Code> Builtins::CallBoundFunction(TailCallMode tail_call_mode) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      return CallBoundFunction();
    case TailCallMode::kAllow:
      return TailCallBoundFunction();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

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

}  // namespace internal
}  // namespace v8
