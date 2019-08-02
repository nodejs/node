// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/execution/isolate.h"
#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

Handle<Code> Builtins::CallFunction(ConvertReceiverMode mode) {
  switch (mode) {
    case ConvertReceiverMode::kNullOrUndefined:
      return builtin_handle(kCallFunction_ReceiverIsNullOrUndefined);
    case ConvertReceiverMode::kNotNullOrUndefined:
      return builtin_handle(kCallFunction_ReceiverIsNotNullOrUndefined);
    case ConvertReceiverMode::kAny:
      return builtin_handle(kCallFunction_ReceiverIsAny);
  }
  UNREACHABLE();
}

Handle<Code> Builtins::Call(ConvertReceiverMode mode) {
  switch (mode) {
    case ConvertReceiverMode::kNullOrUndefined:
      return builtin_handle(kCall_ReceiverIsNullOrUndefined);
    case ConvertReceiverMode::kNotNullOrUndefined:
      return builtin_handle(kCall_ReceiverIsNotNullOrUndefined);
    case ConvertReceiverMode::kAny:
      return builtin_handle(kCall_ReceiverIsAny);
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
