// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

Handle<Code> Builtins::InterpreterPushArgsAndCall(
    TailCallMode tail_call_mode, InterpreterPushArgsMode mode) {
  switch (mode) {
    case InterpreterPushArgsMode::kJSFunction:
      if (tail_call_mode == TailCallMode::kDisallow) {
        return InterpreterPushArgsAndCallFunction();
      } else {
        return InterpreterPushArgsAndTailCallFunction();
      }
    case InterpreterPushArgsMode::kWithFinalSpread:
      CHECK(tail_call_mode == TailCallMode::kDisallow);
      return InterpreterPushArgsAndCallWithFinalSpread();
    case InterpreterPushArgsMode::kOther:
      if (tail_call_mode == TailCallMode::kDisallow) {
        return InterpreterPushArgsAndCall();
      } else {
        return InterpreterPushArgsAndTailCall();
      }
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

void Builtins::Generate_InterpreterPushArgsAndCall(MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(
      masm, TailCallMode::kDisallow, InterpreterPushArgsMode::kOther);
}

void Builtins::Generate_InterpreterPushArgsAndCallFunction(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(
      masm, TailCallMode::kDisallow, InterpreterPushArgsMode::kJSFunction);
}

void Builtins::Generate_InterpreterPushArgsAndCallWithFinalSpread(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(
      masm, TailCallMode::kDisallow, InterpreterPushArgsMode::kWithFinalSpread);
}

void Builtins::Generate_InterpreterPushArgsAndTailCall(MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(
      masm, TailCallMode::kAllow, InterpreterPushArgsMode::kOther);
}

void Builtins::Generate_InterpreterPushArgsAndTailCallFunction(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndCallImpl(
      masm, TailCallMode::kAllow, InterpreterPushArgsMode::kJSFunction);
}

Handle<Code> Builtins::InterpreterPushArgsAndConstruct(
    InterpreterPushArgsMode mode) {
  switch (mode) {
    case InterpreterPushArgsMode::kJSFunction:
      return InterpreterPushArgsAndConstructFunction();
    case InterpreterPushArgsMode::kWithFinalSpread:
      return InterpreterPushArgsAndConstructWithFinalSpread();
    case InterpreterPushArgsMode::kOther:
      return InterpreterPushArgsAndConstruct();
  }
  UNREACHABLE();
  return Handle<Code>::null();
}

void Builtins::Generate_InterpreterPushArgsAndConstruct(MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndConstructImpl(
      masm, InterpreterPushArgsMode::kOther);
}

void Builtins::Generate_InterpreterPushArgsAndConstructWithFinalSpread(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndConstructImpl(
      masm, InterpreterPushArgsMode::kWithFinalSpread);
}

void Builtins::Generate_InterpreterPushArgsAndConstructFunction(
    MacroAssembler* masm) {
  return Generate_InterpreterPushArgsAndConstructImpl(
      masm, InterpreterPushArgsMode::kJSFunction);
}

}  // namespace internal
}  // namespace v8
