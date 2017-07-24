// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_INTRINSICS_H_
#define V8_INTERPRETER_INTERPRETER_INTRINSICS_H_

#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {
namespace interpreter {

// List of supported intrisics, with upper case name, lower case name and
// expected number of arguments (-1 denoting argument count is variable).
#define INTRINSICS_LIST(V)                                           \
  V(AsyncGeneratorGetAwaitInputOrDebugPos,                           \
    async_generator_get_await_input_or_debug_pos, 1)                 \
  V(AsyncGeneratorReject, async_generator_reject, 2)                 \
  V(AsyncGeneratorResolve, async_generator_resolve, 3)               \
  V(Call, call, -1)                                                  \
  V(ClassOf, class_of, 1)                                            \
  V(CreateIterResultObject, create_iter_result_object, 2)            \
  V(CreateAsyncFromSyncIterator, create_async_from_sync_iterator, 1) \
  V(HasProperty, has_property, 2)                                    \
  V(IsArray, is_array, 1)                                            \
  V(IsJSProxy, is_js_proxy, 1)                                       \
  V(IsJSReceiver, is_js_receiver, 1)                                 \
  V(IsSmi, is_smi, 1)                                                \
  V(IsTypedArray, is_typed_array, 1)                                 \
  V(SubString, sub_string, 3)                                        \
  V(ToString, to_string, 1)                                          \
  V(ToLength, to_length, 1)                                          \
  V(ToInteger, to_integer, 1)                                        \
  V(ToNumber, to_number, 1)                                          \
  V(ToObject, to_object, 1)

class IntrinsicsHelper {
 public:
  enum class IntrinsicId {
#define DECLARE_INTRINSIC_ID(name, lower_case, count) k##name,
    INTRINSICS_LIST(DECLARE_INTRINSIC_ID)
#undef DECLARE_INTRINSIC_ID
        kIdCount
  };
  STATIC_ASSERT(static_cast<uint32_t>(IntrinsicId::kIdCount) <= kMaxUInt8);

  static bool IsSupported(Runtime::FunctionId function_id);
  static IntrinsicId FromRuntimeId(Runtime::FunctionId function_id);
  static Runtime::FunctionId ToRuntimeId(IntrinsicId intrinsic_id);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(IntrinsicsHelper);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif
