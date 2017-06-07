// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-intrinsics.h"

#include "src/base/logging.h"

namespace v8 {
namespace internal {
namespace interpreter {

// static
bool IntrinsicsHelper::IsSupported(Runtime::FunctionId function_id) {
  switch (function_id) {
#define SUPPORTED(name, lower_case, count) case Runtime::kInline##name:
    INTRINSICS_LIST(SUPPORTED)
    return true;
#undef SUPPORTED
    default:
      return false;
  }
}

// static
IntrinsicsHelper::IntrinsicId IntrinsicsHelper::FromRuntimeId(
    Runtime::FunctionId function_id) {
  switch (function_id) {
#define TO_RUNTIME_ID(name, lower_case, count) \
  case Runtime::kInline##name:                 \
    return IntrinsicId::k##name;
    INTRINSICS_LIST(TO_RUNTIME_ID)
#undef TO_RUNTIME_ID
    default:
      UNREACHABLE();
      return static_cast<IntrinsicsHelper::IntrinsicId>(-1);
  }
}

// static
Runtime::FunctionId IntrinsicsHelper::ToRuntimeId(
    IntrinsicsHelper::IntrinsicId intrinsic_id) {
  switch (intrinsic_id) {
#define TO_INTRINSIC_ID(name, lower_case, count) \
  case IntrinsicId::k##name:                     \
    return Runtime::kInline##name;
    INTRINSICS_LIST(TO_INTRINSIC_ID)
#undef TO_INTRINSIC_ID
    default:
      UNREACHABLE();
      return static_cast<Runtime::FunctionId>(-1);
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
