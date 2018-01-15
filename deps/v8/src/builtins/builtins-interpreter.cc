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
          return builtin_handle(
              kInterpreterPushUndefinedAndArgsThenCallFunction);
        case ConvertReceiverMode::kNotNullOrUndefined:
        case ConvertReceiverMode::kAny:
          return builtin_handle(kInterpreterPushArgsThenCallFunction);
      }
    case InterpreterPushArgsMode::kWithFinalSpread:
      return builtin_handle(kInterpreterPushArgsThenCallWithFinalSpread);
    case InterpreterPushArgsMode::kOther:
      switch (receiver_mode) {
        case ConvertReceiverMode::kNullOrUndefined:
          return builtin_handle(kInterpreterPushUndefinedAndArgsThenCall);
        case ConvertReceiverMode::kNotNullOrUndefined:
        case ConvertReceiverMode::kAny:
          return builtin_handle(kInterpreterPushArgsThenCall);
      }
  }
  UNREACHABLE();
}

Handle<Code> Builtins::InterpreterPushArgsThenConstruct(
    InterpreterPushArgsMode mode) {
  switch (mode) {
    case InterpreterPushArgsMode::kJSFunction:
      return builtin_handle(kInterpreterPushArgsThenConstructFunction);
    case InterpreterPushArgsMode::kWithFinalSpread:
      return builtin_handle(kInterpreterPushArgsThenConstructWithFinalSpread);
    case InterpreterPushArgsMode::kOther:
      return builtin_handle(kInterpreterPushArgsThenConstruct);
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
