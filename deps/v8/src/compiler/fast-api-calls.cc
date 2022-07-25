// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/fast-api-calls.h"

#include "src/compiler/globals.h"

namespace v8 {
namespace internal {
namespace compiler {
namespace fast_api_call {

ElementsKind GetTypedArrayElementsKind(CTypeInfo::Type type) {
  switch (type) {
    case CTypeInfo::Type::kInt32:
      return INT32_ELEMENTS;
    case CTypeInfo::Type::kUint32:
      return UINT32_ELEMENTS;
    case CTypeInfo::Type::kInt64:
      return BIGINT64_ELEMENTS;
    case CTypeInfo::Type::kUint64:
      return BIGUINT64_ELEMENTS;
    case CTypeInfo::Type::kFloat32:
      return FLOAT32_ELEMENTS;
    case CTypeInfo::Type::kFloat64:
      return FLOAT64_ELEMENTS;
    case CTypeInfo::Type::kVoid:
    case CTypeInfo::Type::kBool:
    case CTypeInfo::Type::kV8Value:
    case CTypeInfo::Type::kApiObject:
    case CTypeInfo::Type::kAny:
      UNREACHABLE();
  }
}

OverloadsResolutionResult ResolveOverloads(
    Zone* zone, const FastApiCallFunctionVector& candidates,
    unsigned int arg_count) {
  DCHECK_GT(arg_count, 0);

  static constexpr int kReceiver = 1;

  // Only the case of the overload resolution of two functions, one with a
  // JSArray param and the other with a typed array param is currently
  // supported.
  DCHECK_EQ(candidates.size(), 2);

  for (unsigned int arg_index = 0; arg_index < arg_count - kReceiver;
       arg_index++) {
    int index_of_func_with_js_array_arg = -1;
    int index_of_func_with_typed_array_arg = -1;
    CTypeInfo::Type element_type = CTypeInfo::Type::kVoid;

    for (size_t i = 0; i < candidates.size(); i++) {
      const CTypeInfo& type_info =
          candidates[i].signature->ArgumentInfo(arg_index + kReceiver);
      CTypeInfo::SequenceType sequence_type = type_info.GetSequenceType();

      if (sequence_type == CTypeInfo::SequenceType::kIsSequence) {
        DCHECK_LT(index_of_func_with_js_array_arg, 0);
        index_of_func_with_js_array_arg = static_cast<int>(i);
      } else if (sequence_type == CTypeInfo::SequenceType::kIsTypedArray) {
        DCHECK_LT(index_of_func_with_typed_array_arg, 0);
        index_of_func_with_typed_array_arg = static_cast<int>(i);
        element_type = type_info.GetType();
      } else {
        DCHECK_LT(index_of_func_with_js_array_arg, 0);
        DCHECK_LT(index_of_func_with_typed_array_arg, 0);
      }
    }

    if (index_of_func_with_js_array_arg >= 0 &&
        index_of_func_with_typed_array_arg >= 0) {
      return {static_cast<int>(arg_index), element_type};
    }
  }

  // No overload found with a JSArray and a typed array as i-th argument.
  return OverloadsResolutionResult::Invalid();
}

bool CanOptimizeFastSignature(const CFunctionInfo* c_signature) {
  USE(c_signature);

#ifndef V8_ENABLE_FP_PARAMS_IN_C_LINKAGE
  if (c_signature->ReturnInfo().GetType() == CTypeInfo::Type::kFloat32 ||
      c_signature->ReturnInfo().GetType() == CTypeInfo::Type::kFloat64) {
    return false;
  }
#endif

#ifndef V8_TARGET_ARCH_64_BIT
  if (c_signature->ReturnInfo().GetType() == CTypeInfo::Type::kInt64 ||
      c_signature->ReturnInfo().GetType() == CTypeInfo::Type::kUint64) {
    return false;
  }
#endif

  for (unsigned int i = 0; i < c_signature->ArgumentCount(); ++i) {
    USE(i);

#ifndef V8_ENABLE_FP_PARAMS_IN_C_LINKAGE
    if (c_signature->ArgumentInfo(i).GetType() == CTypeInfo::Type::kFloat32 ||
        c_signature->ArgumentInfo(i).GetType() == CTypeInfo::Type::kFloat64) {
      return false;
    }
#endif

#ifndef V8_TARGET_ARCH_64_BIT
    if (c_signature->ArgumentInfo(i).GetType() == CTypeInfo::Type::kInt64 ||
        c_signature->ArgumentInfo(i).GetType() == CTypeInfo::Type::kUint64) {
      return false;
    }
#endif
  }

  return true;
}

}  // namespace fast_api_call
}  // namespace compiler
}  // namespace internal
}  // namespace v8
