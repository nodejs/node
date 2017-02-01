// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"
#include "src/builtins/builtins-utils.h"

namespace v8 {
namespace internal {

Handle<Code> Builtins::InterpreterPushArgsAndCall(TailCallMode tail_call_mode,
                                                  CallableType function_type) {
  switch (tail_call_mode) {
    case TailCallMode::kDisallow:
      if (function_type == CallableType::kJSFunction) {
        return InterpreterPushArgsAndCallFunction();
      } else {
        return InterpreterPushArgsAndCall();
      }
    case TailCallMode::kAllow:
      if (function_type == CallableType::kJSFunction) {
        return InterpreterPushArgsAndTailCallFunction();
      } else {
        return InterpreterPushArgsAndTailCall();
      }
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

void Builtins::Generate_InterpreterPushArgsAndCall(MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(masm, TailCallMode::kDisallow,
                                                 CallableType::kAny);
}

void Builtins::Generate_InterpreterPushArgsAndCallFunction(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(masm, TailCallMode::kDisallow,
                                                 CallableType::kJSFunction);
}

void Builtins::Generate_InterpreterPushArgsAndTailCall(MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(masm, TailCallMode::kAllow,
                                                 CallableType::kAny);
}

void Builtins::Generate_InterpreterPushArgsAndTailCallFunction(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(masm, TailCallMode::kAllow,
                                                 CallableType::kJSFunction);
}

Handle<Code> Builtins::InterpreterPushArgsAndConstruct(
    CallableType function_type) {
  switch (function_type) {
    case CallableType::kJSFunction:
      return InterpreterPushArgsAndConstructFunction();
    case CallableType::kAny:
      return InterpreterPushArgsAndConstruct();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

void Builtins::Generate_InterpreterPushArgsAndConstruct(MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndConstructImpl(masm, CallableType::kAny);
}

void Builtins::Generate_InterpreterPushArgsAndConstructFunction(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndConstructImpl(
      masm, CallableType::kJSFunction);
}

}  // namespace internal
}  // namespace v8
