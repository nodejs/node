// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/objects-inl.h"

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

}  // namespace internal
}  // namespace v8
