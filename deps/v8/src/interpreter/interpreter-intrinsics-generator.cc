// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-intrinsics-generator.h"

#include "src/allocation.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/frames.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-assembler.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/objects-inl.h"
#include "src/objects/module.h"

namespace v8 {
namespace internal {
namespace interpreter {

using compiler::Node;

class IntrinsicsGenerator {
 public:
  explicit IntrinsicsGenerator(InterpreterAssembler* assembler)
      : isolate_(assembler->isolate()),
        zone_(assembler->zone()),
        assembler_(assembler) {}

  Node* InvokeIntrinsic(Node* function_id, Node* context, Node* first_arg_reg,
                        Node* arg_count);

 private:
  enum InstanceTypeCompareMode {
    kInstanceTypeEqual,
    kInstanceTypeGreaterThanOrEqual
  };

  Node* IsInstanceType(Node* input, int type);
  Node* CompareInstanceType(Node* map, int type, InstanceTypeCompareMode mode);
  Node* IntrinsicAsStubCall(Node* input, Node* context,
                            Callable const& callable);
  Node* IntrinsicAsBuiltinCall(Node* input, Node* context, Builtins::Name name);
  void AbortIfArgCountMismatch(int expected, compiler::Node* actual);

#define DECLARE_INTRINSIC_HELPER(name, lower_case, count) \
  Node* name(Node* input, Node* arg_count, Node* context);
  INTRINSICS_LIST(DECLARE_INTRINSIC_HELPER)
#undef DECLARE_INTRINSIC_HELPER

  Isolate* isolate() { return isolate_; }
  Zone* zone() { return zone_; }

  Isolate* isolate_;
  Zone* zone_;
  InterpreterAssembler* assembler_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicsGenerator);
};

Node* GenerateInvokeIntrinsic(InterpreterAssembler* assembler,
                              Node* function_id, Node* context,
                              Node* first_arg_reg, Node* arg_count) {
  IntrinsicsGenerator generator(assembler);
  return generator.InvokeIntrinsic(function_id, context, first_arg_reg,
                                   arg_count);
}

#define __ assembler_->

Node* IntrinsicsGenerator::InvokeIntrinsic(Node* function_id, Node* context,
                                           Node* first_arg_reg,
                                           Node* arg_count) {
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
  static_cast<int32_t>(IntrinsicsHelper::IntrinsicId::k##name),
  int32_t cases[] = {INTRINSICS_LIST(CASE)};
#undef CASE

  __ Switch(function_id, &abort, cases, labels, arraysize(cases));
#define HANDLE_CASE(name, lower_case, expected_arg_count)     \
  __ BIND(&lower_case);                                       \
  {                                                           \
    if (FLAG_debug_code && expected_arg_count >= 0) {         \
      AbortIfArgCountMismatch(expected_arg_count, arg_count); \
    }                                                         \
    Node* value = name(first_arg_reg, arg_count, context);    \
    if (value) {                                              \
      result.Bind(value);                                     \
      __ Goto(&end);                                          \
    }                                                         \
  }
  INTRINSICS_LIST(HANDLE_CASE)
#undef HANDLE_CASE

  __ BIND(&abort);
  {
    __ Abort(BailoutReason::kUnexpectedFunctionIDForInvokeIntrinsic);
    result.Bind(__ UndefinedConstant());
    __ Goto(&end);
  }

  __ BIND(&end);
  return result.value();
}

Node* IntrinsicsGenerator::CompareInstanceType(Node* object, int type,
                                               InstanceTypeCompareMode mode) {
  Node* instance_type = __ LoadInstanceType(object);

  if (mode == kInstanceTypeEqual) {
    return __ Word32Equal(instance_type, __ Int32Constant(type));
  } else {
    DCHECK_EQ(mode, kInstanceTypeGreaterThanOrEqual);
    return __ Int32GreaterThanOrEqual(instance_type, __ Int32Constant(type));
  }
}

Node* IntrinsicsGenerator::IsInstanceType(Node* input, int type) {
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  // TODO(ishell): Use Select here.
  InterpreterAssembler::Label if_not_smi(assembler_), return_true(assembler_),
      return_false(assembler_), end(assembler_);
  Node* arg = __ LoadRegister(input);
  __ GotoIf(__ TaggedIsSmi(arg), &return_false);

  Node* condition = CompareInstanceType(arg, type, kInstanceTypeEqual);
  __ Branch(condition, &return_true, &return_false);

  __ BIND(&return_true);
  {
    return_value.Bind(__ TrueConstant());
    __ Goto(&end);
  }

  __ BIND(&return_false);
  {
    return_value.Bind(__ FalseConstant());
    __ Goto(&end);
  }

  __ BIND(&end);
  return return_value.value();
}

Node* IntrinsicsGenerator::IsJSReceiver(Node* input, Node* arg_count,
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

  __ BIND(&return_true);
  {
    return_value.Bind(__ TrueConstant());
    __ Goto(&end);
  }

  __ BIND(&return_false);
  {
    return_value.Bind(__ FalseConstant());
    __ Goto(&end);
  }

  __ BIND(&end);
  return return_value.value();
}

Node* IntrinsicsGenerator::IsArray(Node* input, Node* arg_count,
                                   Node* context) {
  return IsInstanceType(input, JS_ARRAY_TYPE);
}

Node* IntrinsicsGenerator::IsJSProxy(Node* input, Node* arg_count,
                                     Node* context) {
  return IsInstanceType(input, JS_PROXY_TYPE);
}

Node* IntrinsicsGenerator::IsTypedArray(Node* input, Node* arg_count,
                                        Node* context) {
  return IsInstanceType(input, JS_TYPED_ARRAY_TYPE);
}

Node* IntrinsicsGenerator::IsJSMap(Node* input, Node* arg_count,
                                   Node* context) {
  return IsInstanceType(input, JS_MAP_TYPE);
}

Node* IntrinsicsGenerator::IsJSSet(Node* input, Node* arg_count,
                                   Node* context) {
  return IsInstanceType(input, JS_SET_TYPE);
}

Node* IntrinsicsGenerator::IsJSWeakMap(Node* input, Node* arg_count,
                                       Node* context) {
  return IsInstanceType(input, JS_WEAK_MAP_TYPE);
}

Node* IntrinsicsGenerator::IsJSWeakSet(Node* input, Node* arg_count,
                                       Node* context) {
  return IsInstanceType(input, JS_WEAK_SET_TYPE);
}

Node* IntrinsicsGenerator::IsSmi(Node* input, Node* arg_count, Node* context) {
  // TODO(ishell): Use SelectBooleanConstant here.
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  InterpreterAssembler::Label if_smi(assembler_), if_not_smi(assembler_),
      end(assembler_);

  Node* arg = __ LoadRegister(input);

  __ Branch(__ TaggedIsSmi(arg), &if_smi, &if_not_smi);
  __ BIND(&if_smi);
  {
    return_value.Bind(__ TrueConstant());
    __ Goto(&end);
  }

  __ BIND(&if_not_smi);
  {
    return_value.Bind(__ FalseConstant());
    __ Goto(&end);
  }

  __ BIND(&end);
  return return_value.value();
}

Node* IntrinsicsGenerator::IntrinsicAsStubCall(Node* args_reg, Node* context,
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

Node* IntrinsicsGenerator::IntrinsicAsBuiltinCall(Node* input, Node* context,
                                                  Builtins::Name name) {
  Callable callable = Builtins::CallableFor(isolate_, name);
  return IntrinsicAsStubCall(input, context, callable);
}

Node* IntrinsicsGenerator::CreateIterResultObject(Node* input, Node* arg_count,
                                                  Node* context) {
  return IntrinsicAsStubCall(
      input, context,
      Builtins::CallableFor(isolate(), Builtins::kCreateIterResultObject));
}

Node* IntrinsicsGenerator::HasProperty(Node* input, Node* arg_count,
                                       Node* context) {
  return IntrinsicAsStubCall(
      input, context, Builtins::CallableFor(isolate(), Builtins::kHasProperty));
}

Node* IntrinsicsGenerator::ToString(Node* input, Node* arg_count,
                                    Node* context) {
  return IntrinsicAsStubCall(
      input, context, Builtins::CallableFor(isolate(), Builtins::kToString));
}

Node* IntrinsicsGenerator::ToLength(Node* input, Node* arg_count,
                                    Node* context) {
  return IntrinsicAsStubCall(
      input, context, Builtins::CallableFor(isolate(), Builtins::kToLength));
}

Node* IntrinsicsGenerator::ToInteger(Node* input, Node* arg_count,
                                     Node* context) {
  return IntrinsicAsStubCall(
      input, context, Builtins::CallableFor(isolate(), Builtins::kToInteger));
}

Node* IntrinsicsGenerator::ToNumber(Node* input, Node* arg_count,
                                    Node* context) {
  return IntrinsicAsStubCall(
      input, context, Builtins::CallableFor(isolate(), Builtins::kToNumber));
}

Node* IntrinsicsGenerator::ToObject(Node* input, Node* arg_count,
                                    Node* context) {
  return IntrinsicAsStubCall(
      input, context, Builtins::CallableFor(isolate(), Builtins::kToObject));
}

Node* IntrinsicsGenerator::Call(Node* args_reg, Node* arg_count,
                                Node* context) {
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
    __ GotoIfNot(comparison, &arg_count_positive);
    __ Abort(kWrongArgumentCountForInvokeIntrinsic);
    __ Goto(&arg_count_positive);
    __ BIND(&arg_count_positive);
  }

  __ CallJSAndDispatch(function, context, receiver_arg, target_args_count,
                       ConvertReceiverMode::kAny);
  return nullptr;  // We never return from the CallJSAndDispatch above.
}

Node* IntrinsicsGenerator::ClassOf(Node* args_reg, Node* arg_count,
                                   Node* context) {
  Node* value = __ LoadRegister(args_reg);
  return __ ClassOf(value);
}

Node* IntrinsicsGenerator::CreateAsyncFromSyncIterator(Node* args_reg,
                                                       Node* arg_count,
                                                       Node* context) {
  InterpreterAssembler::Label not_receiver(
      assembler_, InterpreterAssembler::Label::kDeferred);
  InterpreterAssembler::Label done(assembler_);
  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);

  Node* sync_iterator = __ LoadRegister(args_reg);

  __ GotoIf(__ TaggedIsSmi(sync_iterator), &not_receiver);
  __ GotoIfNot(__ IsJSReceiver(sync_iterator), &not_receiver);

  Node* const native_context = __ LoadNativeContext(context);
  Node* const map = __ LoadContextElement(
      native_context, Context::ASYNC_FROM_SYNC_ITERATOR_MAP_INDEX);
  Node* const iterator = __ AllocateJSObjectFromMap(map);

  __ StoreObjectFieldNoWriteBarrier(
      iterator, JSAsyncFromSyncIterator::kSyncIteratorOffset, sync_iterator);

  return_value.Bind(iterator);
  __ Goto(&done);

  __ BIND(&not_receiver);
  {
    return_value.Bind(
        __ CallRuntime(Runtime::kThrowSymbolIteratorInvalid, context));

    // Unreachable due to the Throw in runtime call.
    __ Goto(&done);
  }

  __ BIND(&done);
  return return_value.value();
}

Node* IntrinsicsGenerator::CreateJSGeneratorObject(Node* input, Node* arg_count,
                                                   Node* context) {
  return IntrinsicAsBuiltinCall(input, context,
                                Builtins::kCreateGeneratorObject);
}

Node* IntrinsicsGenerator::GeneratorGetContext(Node* args_reg, Node* arg_count,
                                               Node* context) {
  Node* generator = __ LoadRegister(args_reg);
  Node* const value =
      __ LoadObjectField(generator, JSGeneratorObject::kContextOffset);

  return value;
}

Node* IntrinsicsGenerator::GeneratorGetInputOrDebugPos(Node* args_reg,
                                                       Node* arg_count,
                                                       Node* context) {
  Node* generator = __ LoadRegister(args_reg);
  Node* const value =
      __ LoadObjectField(generator, JSGeneratorObject::kInputOrDebugPosOffset);

  return value;
}

Node* IntrinsicsGenerator::GeneratorGetResumeMode(Node* args_reg,
                                                  Node* arg_count,
                                                  Node* context) {
  Node* generator = __ LoadRegister(args_reg);
  Node* const value =
      __ LoadObjectField(generator, JSGeneratorObject::kResumeModeOffset);

  return value;
}

Node* IntrinsicsGenerator::GeneratorClose(Node* args_reg, Node* arg_count,
                                          Node* context) {
  Node* generator = __ LoadRegister(args_reg);
  __ StoreObjectFieldNoWriteBarrier(
      generator, JSGeneratorObject::kContinuationOffset,
      __ SmiConstant(JSGeneratorObject::kGeneratorClosed));
  return __ UndefinedConstant();
}

Node* IntrinsicsGenerator::GetImportMetaObject(Node* args_reg, Node* arg_count,
                                               Node* context) {
  Node* const module_context = __ LoadModuleContext(context);
  Node* const module =
      __ LoadContextElement(module_context, Context::EXTENSION_INDEX);
  Node* const import_meta =
      __ LoadObjectField(module, Module::kImportMetaOffset);

  InterpreterAssembler::Variable return_value(assembler_,
                                              MachineRepresentation::kTagged);
  return_value.Bind(import_meta);

  InterpreterAssembler::Label end(assembler_);
  __ GotoIfNot(__ IsTheHole(import_meta), &end);

  return_value.Bind(__ CallRuntime(Runtime::kGetImportMetaObject, context));
  __ Goto(&end);

  __ BIND(&end);
  return return_value.value();
}

Node* IntrinsicsGenerator::AsyncGeneratorReject(Node* input, Node* arg_count,
                                                Node* context) {
  return IntrinsicAsBuiltinCall(input, context,
                                Builtins::kAsyncGeneratorReject);
}

Node* IntrinsicsGenerator::AsyncGeneratorResolve(Node* input, Node* arg_count,
                                                 Node* context) {
  return IntrinsicAsBuiltinCall(input, context,
                                Builtins::kAsyncGeneratorResolve);
}

Node* IntrinsicsGenerator::AsyncGeneratorYield(Node* input, Node* arg_count,
                                               Node* context) {
  return IntrinsicAsBuiltinCall(input, context, Builtins::kAsyncGeneratorYield);
}

void IntrinsicsGenerator::AbortIfArgCountMismatch(int expected, Node* actual) {
  InterpreterAssembler::Label match(assembler_);
  Node* comparison = __ Word32Equal(actual, __ Int32Constant(expected));
  __ GotoIf(comparison, &match);
  __ Abort(kWrongArgumentCountForInvokeIntrinsic);
  __ Goto(&match);
  __ BIND(&match);
}

#undef __

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
