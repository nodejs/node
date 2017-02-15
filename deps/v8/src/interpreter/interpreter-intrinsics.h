// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERPRETER_INTERPRETER_INTRINSICS_H_
#define V8_INTERPRETER_INTERPRETER_INTRINSICS_H_

#include "src/allocation.h"
#include "src/builtins/builtins.h"
#include "src/frames.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

namespace compiler {
class Node;
}  // namespace compiler

namespace interpreter {

// List of supported intrisics, with upper case name, lower case name and
// expected number of arguments (-1 denoting argument count is variable).
#define INTRINSICS_LIST(V)                              \
  V(Call, call, -1)                                     \
  V(ClassOf, class_of, 1)                               \
  V(HasProperty, has_property, 2)                       \
  V(IsArray, is_array, 1)                               \
  V(IsJSProxy, is_js_proxy, 1)                          \
  V(IsJSReceiver, is_js_receiver, 1)                    \
  V(IsRegExp, is_regexp, 1)                             \
  V(IsSmi, is_smi, 1)                                   \
  V(IsTypedArray, is_typed_array, 1)                    \
  V(NewObject, new_object, 2)                           \
  V(NumberToString, number_to_string, 1)                \
  V(RegExpConstructResult, reg_exp_construct_result, 3) \
  V(RegExpExec, reg_exp_exec, 4)                        \
  V(SubString, sub_string, 3)                           \
  V(ToString, to_string, 1)                             \
  V(ToLength, to_length, 1)                             \
  V(ToInteger, to_integer, 1)                           \
  V(ToNumber, to_number, 1)                             \
  V(ToObject, to_object, 1)                             \
  V(ValueOf, value_of, 1)

class IntrinsicsHelper {
 public:
  enum class IntrinsicId {
#define DECLARE_INTRINSIC_ID(name, lower_case, count) k##name,
    INTRINSICS_LIST(DECLARE_INTRINSIC_ID)
#undef DECLARE_INTRINSIC_ID
        kIdCount
  };
  STATIC_ASSERT(static_cast<uint32_t>(IntrinsicId::kIdCount) <= kMaxUInt8);

  explicit IntrinsicsHelper(InterpreterAssembler* assembler);

  compiler::Node* InvokeIntrinsic(compiler::Node* function_id,
                                  compiler::Node* context,
                                  compiler::Node* first_arg_reg,
                                  compiler::Node* arg_count);

  static bool IsSupported(Runtime::FunctionId function_id);
  static IntrinsicId FromRuntimeId(Runtime::FunctionId function_id);
  static Runtime::FunctionId ToRuntimeId(IntrinsicId intrinsic_id);

 private:
  enum InstanceTypeCompareMode {
    kInstanceTypeEqual,
    kInstanceTypeGreaterThanOrEqual
  };

  compiler::Node* IsInstanceType(compiler::Node* input, int type);
  compiler::Node* CompareInstanceType(compiler::Node* map, int type,
                                      InstanceTypeCompareMode mode);
  compiler::Node* IntrinsicAsStubCall(compiler::Node* input,
                                      compiler::Node* context,
                                      Callable const& callable);
  void AbortIfArgCountMismatch(int expected, compiler::Node* actual);

#define DECLARE_INTRINSIC_HELPER(name, lower_case, count)                \
  compiler::Node* name(compiler::Node* input, compiler::Node* arg_count, \
                       compiler::Node* context);
  INTRINSICS_LIST(DECLARE_INTRINSIC_HELPER)
#undef DECLARE_INTRINSIC_HELPER

  Isolate* isolate() { return isolate_; }
  Zone* zone() { return zone_; }

  Isolate* isolate_;
  Zone* zone_;
  InterpreterAssembler* assembler_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicsHelper);
};

}  // namespace interpreter
}  // namespace internal
}  // namespace v8

#endif
