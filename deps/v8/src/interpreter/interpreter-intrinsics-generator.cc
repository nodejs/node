// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/interpreter/interpreter-intrinsics-generator.h"

#include "src/builtins/builtins.h"
#include "src/codegen/code-factory.h"
#include "src/execution/frames.h"
#include "src/heap/factory-inl.h"
#include "src/interpreter/bytecodes.h"
#include "src/interpreter/interpreter-assembler.h"
#include "src/interpreter/interpreter-intrinsics.h"
#include "src/objects/js-generator.h"
#include "src/objects/objects-inl.h"
#include "src/objects/source-text-module.h"
#include "src/utils/allocation.h"

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

  TNode<Object> InvokeIntrinsic(
      TNode<Uint32T> function_id, TNode<Context> context,
      const InterpreterAssembler::RegListNodePair& args);

 private:
  enum InstanceTypeCompareMode {
    kInstanceTypeEqual,
    kInstanceTypeGreaterThanOrEqual
  };

  TNode<Oddball> IsInstanceType(TNode<Object> input, int type);
  TNode<BoolT> CompareInstanceType(TNode<HeapObject> map, int type,
                                   InstanceTypeCompareMode mode);
  TNode<Object> IntrinsicAsStubCall(
      const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
      Callable const& callable);
  TNode<Object> IntrinsicAsBuiltinCall(
      const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
      Builtins::Name name);
  void AbortIfArgCountMismatch(int expected, TNode<Word32T> actual);

#define DECLARE_INTRINSIC_HELPER(name, lower_case, count)               \
  TNode<Object> name(const InterpreterAssembler::RegListNodePair& args, \
                     TNode<Context> context);
  INTRINSICS_LIST(DECLARE_INTRINSIC_HELPER)
#undef DECLARE_INTRINSIC_HELPER

  Isolate* isolate() { return isolate_; }
  Zone* zone() { return zone_; }
  Factory* factory() { return isolate()->factory(); }

  Isolate* isolate_;
  Zone* zone_;
  InterpreterAssembler* assembler_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicsGenerator);
};

TNode<Object> GenerateInvokeIntrinsic(
    InterpreterAssembler* assembler, TNode<Uint32T> function_id,
    TNode<Context> context, const InterpreterAssembler::RegListNodePair& args) {
  IntrinsicsGenerator generator(assembler);
  return generator.InvokeIntrinsic(function_id, context, args);
}

#define __ assembler_->

TNode<Object> IntrinsicsGenerator::InvokeIntrinsic(
    TNode<Uint32T> function_id, TNode<Context> context,
    const InterpreterAssembler::RegListNodePair& args) {
  InterpreterAssembler::Label abort(assembler_), end(assembler_);
  InterpreterAssembler::TVariable<Object> result(assembler_);

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
#define HANDLE_CASE(name, lower_case, expected_arg_count)            \
  __ BIND(&lower_case);                                              \
  {                                                                  \
    if (FLAG_debug_code && expected_arg_count >= 0) {                \
      AbortIfArgCountMismatch(expected_arg_count, args.reg_count()); \
    }                                                                \
    TNode<Object> value = name(args, context);                       \
    if (value) {                                                     \
      result = value;                                                \
      __ Goto(&end);                                                 \
    }                                                                \
  }
  INTRINSICS_LIST(HANDLE_CASE)
#undef HANDLE_CASE

  __ BIND(&abort);
  {
    __ Abort(AbortReason::kUnexpectedFunctionIDForInvokeIntrinsic);
    result = __ UndefinedConstant();
    __ Goto(&end);
  }

  __ BIND(&end);
  return result.value();
}

TNode<BoolT> IntrinsicsGenerator::CompareInstanceType(
    TNode<HeapObject> object, int type, InstanceTypeCompareMode mode) {
  TNode<Uint16T> instance_type = __ LoadInstanceType(object);

  if (mode == kInstanceTypeEqual) {
    return __ Word32Equal(instance_type, __ Int32Constant(type));
  } else {
    DCHECK_EQ(mode, kInstanceTypeGreaterThanOrEqual);
    return __ Int32GreaterThanOrEqual(instance_type, __ Int32Constant(type));
  }
}

TNode<Oddball> IntrinsicsGenerator::IsInstanceType(TNode<Object> input,
                                                   int type) {
  TNode<Oddball> result = __ Select<Oddball>(
      __ TaggedIsSmi(input), [=] { return __ FalseConstant(); },
      [=] {
        return __ SelectBooleanConstant(
            CompareInstanceType(__ CAST(input), type, kInstanceTypeEqual));
      });
  return result;
}

TNode<Object> IntrinsicsGenerator::IsJSReceiver(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  TNode<Object> input = __ LoadRegisterFromRegisterList(args, 0);
  TNode<Oddball> result = __ Select<Oddball>(
      __ TaggedIsSmi(input), [=] { return __ FalseConstant(); },
      [=] {
        return __ SelectBooleanConstant(__ IsJSReceiver(__ CAST(input)));
      });
  return result;
}

TNode<Object> IntrinsicsGenerator::IsArray(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  TNode<Object> input = __ LoadRegisterFromRegisterList(args, 0);
  return IsInstanceType(input, JS_ARRAY_TYPE);
}

TNode<Object> IntrinsicsGenerator::IsSmi(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  TNode<Object> input = __ LoadRegisterFromRegisterList(args, 0);
  return __ SelectBooleanConstant(__ TaggedIsSmi(input));
}

TNode<Object> IntrinsicsGenerator::IntrinsicAsStubCall(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    Callable const& callable) {
  int param_count = callable.descriptor().GetParameterCount();
  int input_count = param_count + 2;  // +2 for target and context
  Node** stub_args = zone()->NewArray<Node*>(input_count);
  int index = 0;
  stub_args[index++] = __ HeapConstant(callable.code());
  for (int i = 0; i < param_count; i++) {
    stub_args[index++] = __ LoadRegisterFromRegisterList(args, i);
  }
  stub_args[index++] = context;
  return __ CAST(__ CallStubN(StubCallMode::kCallCodeObject,
                              callable.descriptor(), 1, input_count,
                              stub_args));
}

TNode<Object> IntrinsicsGenerator::IntrinsicAsBuiltinCall(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    Builtins::Name name) {
  Callable callable = Builtins::CallableFor(isolate_, name);
  return IntrinsicAsStubCall(args, context, callable);
}

TNode<Object> IntrinsicsGenerator::CopyDataProperties(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsStubCall(
      args, context,
      Builtins::CallableFor(isolate(), Builtins::kCopyDataProperties));
}

TNode<Object> IntrinsicsGenerator::CreateIterResultObject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsStubCall(
      args, context,
      Builtins::CallableFor(isolate(), Builtins::kCreateIterResultObject));
}

TNode<Object> IntrinsicsGenerator::HasProperty(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsStubCall(
      args, context, Builtins::CallableFor(isolate(), Builtins::kHasProperty));
}

TNode<Object> IntrinsicsGenerator::ToString(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsStubCall(
      args, context, Builtins::CallableFor(isolate(), Builtins::kToString));
}

TNode<Object> IntrinsicsGenerator::ToLength(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsStubCall(
      args, context, Builtins::CallableFor(isolate(), Builtins::kToLength));
}

TNode<Object> IntrinsicsGenerator::ToObject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsStubCall(
      args, context, Builtins::CallableFor(isolate(), Builtins::kToObject));
}

TNode<Object> IntrinsicsGenerator::Call(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  // First argument register contains the function target.
  TNode<Object> function = __ LoadRegisterFromRegisterList(args, 0);

  // The arguments for the target function are from the second runtime call
  // argument.
  InterpreterAssembler::RegListNodePair target_args(
      __ RegisterLocationInRegisterList(args, 1),
      __ Int32Sub(args.reg_count(), __ Int32Constant(1)));

  if (FLAG_debug_code) {
    InterpreterAssembler::Label arg_count_positive(assembler_);
    TNode<BoolT> comparison =
        __ Int32LessThan(target_args.reg_count(), __ Int32Constant(0));
    __ GotoIfNot(comparison, &arg_count_positive);
    __ Abort(AbortReason::kWrongArgumentCountForInvokeIntrinsic);
    __ Goto(&arg_count_positive);
    __ BIND(&arg_count_positive);
  }

  __ CallJSAndDispatch(function, context, target_args,
                       ConvertReceiverMode::kAny);
  return TNode<Object>();  // We never return from the CallJSAndDispatch above.
}

TNode<Object> IntrinsicsGenerator::CreateAsyncFromSyncIterator(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  InterpreterAssembler::Label not_receiver(
      assembler_, InterpreterAssembler::Label::kDeferred);
  InterpreterAssembler::Label done(assembler_);
  InterpreterAssembler::TVariable<Object> return_value(assembler_);

  TNode<Object> sync_iterator = __ LoadRegisterFromRegisterList(args, 0);

  __ GotoIf(__ TaggedIsSmi(sync_iterator), &not_receiver);
  __ GotoIfNot(__ IsJSReceiver(__ CAST(sync_iterator)), &not_receiver);

  const TNode<Object> next =
      __ GetProperty(context, sync_iterator, factory()->next_string());

  const TNode<NativeContext> native_context = __ LoadNativeContext(context);
  const TNode<Map> map = __ CAST(__ LoadContextElement(
      native_context, Context::ASYNC_FROM_SYNC_ITERATOR_MAP_INDEX));
  const TNode<JSObject> iterator = __ AllocateJSObjectFromMap(map);

  __ StoreObjectFieldNoWriteBarrier(
      iterator, JSAsyncFromSyncIterator::kSyncIteratorOffset, sync_iterator);
  __ StoreObjectFieldNoWriteBarrier(iterator,
                                    JSAsyncFromSyncIterator::kNextOffset, next);

  return_value = iterator;
  __ Goto(&done);

  __ BIND(&not_receiver);
  {
    return_value =
        __ CallRuntime(Runtime::kThrowSymbolIteratorInvalid, context);

    // Unreachable due to the Throw in runtime call.
    __ Goto(&done);
  }

  __ BIND(&done);
  return return_value.value();
}

TNode<Object> IntrinsicsGenerator::CreateJSGeneratorObject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context,
                                Builtins::kCreateGeneratorObject);
}

TNode<Object> IntrinsicsGenerator::GeneratorGetResumeMode(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  TNode<JSGeneratorObject> generator =
      __ CAST(__ LoadRegisterFromRegisterList(args, 0));
  const TNode<Object> value =
      __ LoadObjectField(generator, JSGeneratorObject::kResumeModeOffset);

  return value;
}

TNode<Object> IntrinsicsGenerator::GeneratorClose(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  TNode<JSGeneratorObject> generator =
      __ CAST(__ LoadRegisterFromRegisterList(args, 0));
  __ StoreObjectFieldNoWriteBarrier(
      generator, JSGeneratorObject::kContinuationOffset,
      __ SmiConstant(JSGeneratorObject::kGeneratorClosed));
  return __ UndefinedConstant();
}

TNode<Object> IntrinsicsGenerator::GetImportMetaObject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  const TNode<Context> module_context = __ LoadModuleContext(context);
  const TNode<HeapObject> module =
      __ CAST(__ LoadContextElement(module_context, Context::EXTENSION_INDEX));
  const TNode<Object> import_meta =
      __ LoadObjectField(module, SourceTextModule::kImportMetaOffset);

  InterpreterAssembler::TVariable<Object> return_value(assembler_);
  return_value = import_meta;

  InterpreterAssembler::Label end(assembler_);
  __ GotoIfNot(__ IsTheHole(import_meta), &end);

  return_value = __ CallRuntime(Runtime::kGetImportMetaObject, context);
  __ Goto(&end);

  __ BIND(&end);
  return return_value.value();
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionAwaitCaught(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context,
                                Builtins::kAsyncFunctionAwaitCaught);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionAwaitUncaught(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context,
                                Builtins::kAsyncFunctionAwaitUncaught);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionEnter(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncFunctionEnter);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionReject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncFunctionReject);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionResolve(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncFunctionResolve);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorAwaitCaught(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context,
                                Builtins::kAsyncGeneratorAwaitCaught);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorAwaitUncaught(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context,
                                Builtins::kAsyncGeneratorAwaitUncaught);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorReject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncGeneratorReject);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorResolve(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context,
                                Builtins::kAsyncGeneratorResolve);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorYield(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncGeneratorYield);
}

void IntrinsicsGenerator::AbortIfArgCountMismatch(int expected,
                                                  TNode<Word32T> actual) {
  InterpreterAssembler::Label match(assembler_);
  TNode<BoolT> comparison = __ Word32Equal(actual, __ Int32Constant(expected));
  __ GotoIf(comparison, &match);
  __ Abort(AbortReason::kWrongArgumentCountForInvokeIntrinsic);
  __ Goto(&match);
  __ BIND(&match);
}

#undef __

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
