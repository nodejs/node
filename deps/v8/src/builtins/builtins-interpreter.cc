// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/globals.h"
#include "src/handles-inl.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

Handle<Code> Builtins::InterpreterPushArgsThenCall(
    ConvertReceiverMode receiver_mode, InterpreterPushArgsMode mode) {
  switch (mode) {
    case InterpreterPushArgsMode::kJSFunction:
      switch (receiver_mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return InterpreterPushUndefinedAndArgsThenCallFunction();
        case ConvertReceiverMode::kNotNullOrUndefined:
        case ConvertReceiverMode::kAny:
          return InterpreterPushArgsThenCallFunction();
      }
    case InterpreterPushArgsMode::kWithFinalSpread:
      return InterpreterPushArgsThenCallWithFinalSpread();
    case InterpreterPushArgsMode::kOther:
      switch (receiver_mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return InterpreterPushUndefinedAndArgsThenCall();
        case ConvertReceiverMode::kNotNullOrUndefined:
        case ConvertReceiverMode::kAny:
          return InterpreterPushArgsThenCall();
      }
  }
  UNREACHABLE();
}

Handle<Code> Builtins::InterpreterPushArgsThenConstruct(
    InterpreterPushArgsMode mode) {
  switch (mode) {
    case InterpreterPushArgsMode::kJSFunction:
      return InterpreterPushArgsThenConstructFunction();
    case InterpreterPushArgsMode::kWithFinalSpread:
      return InterpreterPushArgsThenConstructWithFinalSpread();
    case InterpreterPushArgsMode::kOther:
      return InterpreterPushArgsThenConstruct();
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
