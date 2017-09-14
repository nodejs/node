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

Handle<Code> Builtins::CallFunction(ConvertReceiverMode mode) {
  switch (mode) {
    case ConvertReceiverMode::kNullOrUndefined:
      return CallFunction_ReceiverIsNullOrUndefined();
    case ConvertReceiverMode::kNotNullOrUndefined:
      return CallFunction_ReceiverIsNotNullOrUndefined();
    case ConvertReceiverMode::kAny:
      return CallFunction_ReceiverIsAny();
  }
  UNREACHABLE();
}

Handle<Code> Builtins::Call(ConvertReceiverMode mode) {
  switch (mode) {
    case ConvertReceiverMode::kNullOrUndefined:
      return Call_ReceiverIsNullOrUndefined();
    case ConvertReceiverMode::kNotNullOrUndefined:
      return Call_ReceiverIsNotNullOrUndefined();
    case ConvertReceiverMode::kAny:
      return Call_ReceiverIsAny();
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
