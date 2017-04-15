// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-intrinsics.h"

#include "src/code-factory.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::Node;

#define __ assembler_->

IntrinsicsHelper::IntrinsicsHelper(InterpreterAssembler* assembler)
    : isolate_(assembler->isolate()),
      zone_(assembler->zone()),
      assembler_(assembler) {}

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
  static_cast<int32_t>(IntrinsicId::k##name),
  int32_t cases[] = {INTRINSICS_LIST(CASE)};
#undef CASE

  __ Switch(function_id, &abort, cases, labels, arraysize(cases));
#define HANDLE_CASE(name, lower_case, expected_arg_count)   \
  __ Bind(&lower_case);                                     \
  if (FLAG_debug_code && expected_arg_count >= 0) {         \
    AbortIfArgCountMismatch(expected_arg_count, arg_count); \
  }                                                         \
  result.Bind(name(first_arg_reg, arg_count, context));     \
  __ Goto(&end);
  INTRINSICS_LIST(HANDLE_CASE)
#undef HANDLE_CASE

  __ Bind(&abort);
  {
    __ Abort(BailoutReason::kUnexpectedFunctionIDForInvokeIntrinsic);
    result.Bind(__ UndefinedConstant());
    __ Goto(&end);
  }

  __ Bind(&end);
  return result.value();
}

Node* IntrinsicsHelper::CompareInstanceType(Node* object, int type,
                                            InstanceTypeCompareMode mode) {
  Node* instance_type = __ LoadInstanceType(object);

  if (mode == kInstanceTypeEqual) {
    return __ Word32Equal(instance_type, __ Int32Constant(type));
  } else {
    DCHECK(mode == kInstanceTypeGreaterThanOrEqual);
    return __ Int32GreaterThanOrEqual(instance_type, __ Int32Constant(type));
  }
}

Node* IntrinsicsHelper::IsInstanceType(Node* input, int type) {
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  // TODO(ishell): Use Select here.
  InterpreterAssembler::Label if_not_smi(assembler_), return_true(assembler_),
      return_false(assembler_), end(assembler_);
  Node* arg = __ LoadRegister(input);
  __ GotoIf(__ TaggedIsSmi(arg), &return_false);

  Node* condition = CompareInstanceType(arg, type, kInstanceTypeEqual);
  __ Branch(condition, &return_true, &return_false);

  __ Bind(&return_true);
  {
    return_value.Bind(__ BooleanConstant(true));
    __ Goto(&end);
  }

  __ Bind(&return_false);
  {
    return_value.Bind(__ BooleanConstant(false));
    __ Goto(&end);
  }

  __ Bind(&end);
  return return_value.value();
}

Node* IntrinsicsHelper::IsJSReceiver(Node* input, Node* arg_count,
                                     Node* context) {
  // TODO(ishell): Use Select here.
  // TODO(ishell): Use CSA::IsJSReceiverInstanceType here.
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  InterpreterAssembler::Label return_true(assembler_), return_false(assembler_),
      end(assembler_);

  Node* arg = __ LoadRegister(input);
  __ GotoIf(__ TaggedIsSmi(arg), &return_false);

  STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
  Node* condition = CompareInstanceType(arg, FIRST_JS_RECEIVER_TYPE,
                                        kInstanceTypeGreaterThanOrEqual);
  __ Branch(condition, &return_true, &return_false);

  __ Bind(&return_true);
  {
    return_value.Bind(__ BooleanConstant(true));
    __ Goto(&end);
  }

  __ Bind(&return_false);
  {
    return_value.Bind(__ BooleanConstant(false));
    __ Goto(&end);
  }

  __ Bind(&end);
  return return_value.value();
}

Node* IntrinsicsHelper::IsArray(Node* input, Node* arg_count, Node* context) {
  return IsInstanceType(input, JS_ARRAY_TYPE);
}

Node* IntrinsicsHelper::IsJSProxy(Node* input, Node* arg_count, Node* context) {
  return IsInstanceType(input, JS_PROXY_TYPE);
}

Node* IntrinsicsHelper::IsTypedArray(Node* input, Node* arg_count,
                                     Node* context) {
  return IsInstanceType(input, JS_TYPED_ARRAY_TYPE);
}

Node* IntrinsicsHelper::IsSmi(Node* input, Node* arg_count, Node* context) {
  // TODO(ishell): Use SelectBooleanConstant here.
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  InterpreterAssembler::Label if_smi(assembler_), if_not_smi(assembler_),
      end(assembler_);

  Node* arg = __ LoadRegister(input);

  __ Branch(__ TaggedIsSmi(arg), &if_smi, &if_not_smi);
  __ Bind(&if_smi);
  {
    return_value.Bind(__ BooleanConstant(true));
    __ Goto(&end);
  }

  __ Bind(&if_not_smi);
  {
    return_value.Bind(__ BooleanConstant(false));
    __ Goto(&end);
  }

  __ Bind(&end);
  return return_value.value();
}

Node* IntrinsicsHelper::IntrinsicAsStubCall(Node* args_reg, Node* context,
                                            Callable const& callable) {
  int param_count = callable.descriptor().GetParameterCount();
  int input_count = param_count + 2;  // +2 for target and context
  Node** args = zone()->NewArray<Node*>(input_count);
  int index = 0;
  args[index++] = __ HeapConstant(callable.code());
  for (int i = 0; i < param_count; i++) {
    args[index++] = __ LoadRegister(args_reg);
    args_reg = __ NextRegister(args_reg);
  }
  args[index++] = context;
  return __ CallStubN(callable.descriptor(), 1, input_count, args);
}

Node* IntrinsicsHelper::CreateIterResultObject(Node* input, Node* arg_count,
                                               Node* context) {
  return IntrinsicAsStubCall(input, context,
                             CodeFactory::CreateIterResultObject(isolate()));
}

Node* IntrinsicsHelper::HasProperty(Node* input, Node* arg_count,
                                    Node* context) {
  return IntrinsicAsStubCall(input, context,
                             CodeFactory::HasProperty(isolate()));
}

Node* IntrinsicsHelper::NumberToString(Node* input, Node* arg_count,
                                       Node* context) {
  return IntrinsicAsStubCall(input, context,
                             CodeFactory::NumberToString(isolate()));
}

Node* IntrinsicsHelper::RegExpExec(Node* input, Node* arg_count,
                                   Node* context) {
  return IntrinsicAsStubCall(input, context,
                             CodeFactory::RegExpExec(isolate()));
}

Node* IntrinsicsHelper::SubString(Node* input, Node* arg_count, Node* context) {
  return IntrinsicAsStubCall(input, context, CodeFactory::SubString(isolate()));
}

Node* IntrinsicsHelper::ToString(Node* input, Node* arg_count, Node* context) {
  return IntrinsicAsStubCall(input, context, CodeFactory::ToString(isolate()));
}

Node* IntrinsicsHelper::ToLength(Node* input, Node* arg_count, Node* context) {
  return IntrinsicAsStubCall(input, context, CodeFactory::ToLength(isolate()));
}

Node* IntrinsicsHelper::ToInteger(Node* input, Node* arg_count, Node* context) {
  return IntrinsicAsStubCall(input, context, CodeFactory::ToInteger(isolate()));
}

Node* IntrinsicsHelper::ToNumber(Node* input, Node* arg_count, Node* context) {
  return IntrinsicAsStubCall(input, context, CodeFactory::ToNumber(isolate()));
}

Node* IntrinsicsHelper::ToObject(Node* input, Node* arg_count, Node* context) {
  return IntrinsicAsStubCall(input, context, CodeFactory::ToObject(isolate()));
}

Node* IntrinsicsHelper::Call(Node* args_reg, Node* arg_count, Node* context) {
  // First argument register contains the function target.
  Node* function = __ LoadRegister(args_reg);

  // Receiver is the second runtime call argument.
  Node* receiver_reg = __ NextRegister(args_reg);
  Node* receiver_arg = __ RegisterLocation(receiver_reg);

  // Subtract function and receiver from arg count.
  Node* function_and_receiver_count = __ Int32Constant(2);
  Node* target_args_count = __ Int32Sub(arg_count, function_and_receiver_count);

  if (FLAG_debug_code) {
    InterpreterAssembler::Label arg_count_positive(assembler_);
    Node* comparison = __ Int32LessThan(target_args_count, __ Int32Constant(0));
    __ GotoUnless(comparison, &arg_count_positive);
    __ Abort(kWrongArgumentCountForInvokeIntrinsic);
    __ Goto(&arg_count_positive);
    __ Bind(&arg_count_positive);
  }

  Node* result = __ CallJS(function, context, receiver_arg, target_args_count,
                           TailCallMode::kDisallow);
  return result;
}

Node* IntrinsicsHelper::ValueOf(Node* args_reg, Node* arg_count,
                                Node* context) {
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  InterpreterAssembler::Label done(assembler_);

  Node* object = __ LoadRegister(args_reg);
  return_value.Bind(object);

  // If the object is a smi return the object.
  __ GotoIf(__ TaggedIsSmi(object), &done);

  // If the object is not a value type, return the object.
  Node* condition =
      CompareInstanceType(object, JS_VALUE_TYPE, kInstanceTypeEqual);
  __ GotoUnless(condition, &done);

  // If the object is a value type, return the value field.
  return_value.Bind(__ LoadObjectField(object, JSValue::kValueOffset));
  __ Goto(&done);

  __ Bind(&done);
  return return_value.value();
}

Node* IntrinsicsHelper::ClassOf(Node* args_reg, Node* arg_count,
                                Node* context) {
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  InterpreterAssembler::Label done(assembler_), null(assembler_),
      function(assembler_), non_function_constructor(assembler_);

  Node* object = __ LoadRegister(args_reg);

  // If the object is not a JSReceiver, we return null.
  __ GotoIf(__ TaggedIsSmi(object), &null);
  STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
  Node* is_js_receiver = CompareInstanceType(object, FIRST_JS_RECEIVER_TYPE,
                                             kInstanceTypeGreaterThanOrEqual);
  __ GotoUnless(is_js_receiver, &null);

  // Return 'Function' for JSFunction and JSBoundFunction objects.
  Node* is_function = CompareInstanceType(object, FIRST_FUNCTION_TYPE,
                                          kInstanceTypeGreaterThanOrEqual);
  STATIC_ASSERT(LAST_FUNCTION_TYPE == LAST_TYPE);
  __ GotoIf(is_function, &function);

  // Check if the constructor in the map is a JS function.
  Node* constructor = __ LoadMapConstructor(__ LoadMap(object));
  Node* constructor_is_js_function =
      CompareInstanceType(constructor, JS_FUNCTION_TYPE, kInstanceTypeEqual);
  __ GotoUnless(constructor_is_js_function, &non_function_constructor);

  // Grab the instance class name from the constructor function.
  Node* shared =
      __ LoadObjectField(constructor, JSFunction::kSharedFunctionInfoOffset);
  return_value.Bind(
      __ LoadObjectField(shared, SharedFunctionInfo::kInstanceClassNameOffset));
  __ Goto(&done);

  // Non-JS objects have class null.
  __ Bind(&null);
  {
    return_value.Bind(__ LoadRoot(Heap::kNullValueRootIndex));
    __ Goto(&done);
  }

  // Functions have class 'Function'.
  __ Bind(&function);
  {
    return_value.Bind(__ LoadRoot(Heap::kFunction_stringRootIndex));
    __ Goto(&done);
  }

  // Objects with a non-function constructor have class 'Object'.
  __ Bind(&non_function_constructor);
  {
    return_value.Bind(__ LoadRoot(Heap::kObject_stringRootIndex));
    __ Goto(&done);
  }

  __ Bind(&done);
  return return_value.value();
}

void IntrinsicsHelper::AbortIfArgCountMismatch(int expected, Node* actual) {
  InterpreterAssembler::Label match(assembler_);
  Node* comparison = __ Word32Equal(actual, __ Int32Constant(expected));
  __ GotoIf(comparison, &match);
  __ Abort(kWrongArgumentCountForInvokeIntrinsic);
  __ Goto(&match);
  __ Bind(&match);
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
