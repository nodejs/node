// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-intrinsics.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::Node;

#define __ assembler_->

IntrinsicsHelper::IntrinsicsHelper(InterpreterAssembler* assembler)
    : assembler_(assembler) {}

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

Node* IntrinsicsHelper::InvokeIntrinsic(Node* function_id, Node* context,
                                        Node* first_arg_reg, Node* arg_count) {
  InterpreterAssembler::Label abort(assembler_), end(assembler_);
  InterpreterAssembler::Variable result(assembler_,
                                        MachineRepresentation::kTagged);

#define MAKE_LABEL(name, lower_case, count) \
  InterpreterAssembler::Label lower_case(assembler_);
  INTRINSICS_LIST(MAKE_LABEL)
#undef MAKE_LABEL

#define LABEL_POINTER(name, lower_case, count) &lower_case,
  InterpreterAssembler::Label* labels[] = {INTRINSICS_LIST(LABEL_POINTER)};
#undef LABEL_POINTER

#define CASE(name, lower_case, count) \
  static_cast<int32_t>(Runtime::kInline##name),
  int32_t cases[] = {INTRINSICS_LIST(CASE)};
#undef CASE

  __ Switch(function_id, &abort, cases, labels, arraysize(cases));
#define HANDLE_CASE(name, lower_case, expected_arg_count)   \
  __ Bind(&lower_case);                                     \
  if (FLAG_debug_code) {                                    \
    AbortIfArgCountMismatch(expected_arg_count, arg_count); \
  }                                                         \
  result.Bind(name(first_arg_reg));                         \
  __ Goto(&end);
  INTRINSICS_LIST(HANDLE_CASE)
#undef HANDLE_CASE

  __ Bind(&abort);
  __ Abort(BailoutReason::kUnexpectedFunctionIDForInvokeIntrinsic);
  result.Bind(__ UndefinedConstant());
  __ Goto(&end);

  __ Bind(&end);
  return result.value();
}

Node* IntrinsicsHelper::CompareInstanceType(Node* map, int type,
                                            InstanceTypeCompareMode mode) {
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  Node* instance_type = __ LoadInstanceType(map);

  InterpreterAssembler::Label if_true(assembler_), if_false(assembler_),
      end(assembler_);
  Node* condition;
  if (mode == kInstanceTypeEqual) {
    condition = __ Word32Equal(instance_type, __ Int32Constant(type));
  } else {
    DCHECK(mode == kInstanceTypeGreaterThanOrEqual);
    condition =
        __ Int32GreaterThanOrEqual(instance_type, __ Int32Constant(type));
  }
  __ Branch(condition, &if_true, &if_false);

  __ Bind(&if_true);
  return_value.Bind(__ BooleanConstant(true));
  __ Goto(&end);

  __ Bind(&if_false);
  return_value.Bind(__ BooleanConstant(false));
  __ Goto(&end);

  __ Bind(&end);
  return return_value.value();
}

Node* IntrinsicsHelper::IsJSReceiver(Node* input) {
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);

  InterpreterAssembler::Label if_smi(assembler_), if_not_smi(assembler_),
      end(assembler_);
  Node* arg = __ LoadRegister(input);

  __ Branch(__ WordIsSmi(arg), &if_smi, &if_not_smi);
  __ Bind(&if_smi);
  return_value.Bind(__ BooleanConstant(false));
  __ Goto(&end);

  __ Bind(&if_not_smi);
  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  return_value.Bind(CompareInstanceType(arg, FIRST_JS_RECEIVER_TYPE,
                                        kInstanceTypeGreaterThanOrEqual));
  __ Goto(&end);

  __ Bind(&end);
  return return_value.value();
}

Node* IntrinsicsHelper::IsArray(Node* input) {
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);

  InterpreterAssembler::Label if_smi(assembler_), if_not_smi(assembler_),
      end(assembler_);
  Node* arg = __ LoadRegister(input);

  __ Branch(__ WordIsSmi(arg), &if_smi, &if_not_smi);
  __ Bind(&if_smi);
  return_value.Bind(__ BooleanConstant(false));
  __ Goto(&end);

  __ Bind(&if_not_smi);
  return_value.Bind(
      CompareInstanceType(arg, JS_ARRAY_TYPE, kInstanceTypeEqual));
  __ Goto(&end);

  __ Bind(&end);
  return return_value.value();
}

void IntrinsicsHelper::AbortIfArgCountMismatch(int expected, Node* actual) {
  InterpreterAssembler::Label match(assembler_), mismatch(assembler_),
      end(assembler_);
  Node* comparison = __ Word32Equal(actual, __ Int32Constant(expected));
  __ Branch(comparison, &match, &mismatch);
  __ Bind(&mismatch);
  __ Abort(kWrongArgumentCountForInvokeIntrinsic);
  __ Goto(&end);
  __ Bind(&match);
  __ Goto(&end);
  __ Bind(&end);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
