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

class IntrinsicsGenerator {
 public:
  explicit IntrinsicsGenerator(InterpreterAssembler* assembler)
      : isolate_(assembler->isolate()),
        zone_(assembler->zone()),
        assembler_(assembler) {}
  IntrinsicsGenerator(const IntrinsicsGenerator&) = delete;
  IntrinsicsGenerator& operator=(const IntrinsicsGenerator&) = delete;

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
  TNode<Object> IntrinsicAsBuiltinCall(
      const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
      Builtins::Name name, int arg_count);
  void AbortIfArgCountMismatch(int expected, TNode<Word32T> actual);

#define DECLARE_INTRINSIC_HELPER(name, lower_case, count)               \
  TNode<Object> name(const InterpreterAssembler::RegListNodePair& args, \
                     TNode<Context> context, int arg_count);
  INTRINSICS_LIST(DECLARE_INTRINSIC_HELPER)
#undef DECLARE_INTRINSIC_HELPER

  Isolate* isolate() { return isolate_; }
  Zone* zone() { return zone_; }
  Factory* factory() { return isolate()->factory(); }

  Isolate* isolate_;
  Zone* zone_;
  InterpreterAssembler* assembler_;
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
    TNode<Object> value = name(args, context, expected_arg_count);   \
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

TNode<Object> IntrinsicsGenerator::IntrinsicAsBuiltinCall(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    Builtins::Name name, int arg_count) {
  Callable callable = Builtins::CallableFor(isolate_, name);
  switch (arg_count) {
    case 1:
      return __ CallStub(callable, context,
                         __ LoadRegisterFromRegisterList(args, 0));
      break;
    case 2:
      return __ CallStub(callable, context,
                         __ LoadRegisterFromRegisterList(args, 0),
                         __ LoadRegisterFromRegisterList(args, 1));
      break;
    case 3:
      return __ CallStub(callable, context,
                         __ LoadRegisterFromRegisterList(args, 0),
                         __ LoadRegisterFromRegisterList(args, 1),
                         __ LoadRegisterFromRegisterList(args, 2));
      break;
    default:
      UNREACHABLE();
  }
}

TNode<Object> IntrinsicsGenerator::IsJSReceiver(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  TNode<Object> input = __ LoadRegisterFromRegisterList(args, 0);
  TNode<Oddball> result = __ Select<Oddball>(
      __ TaggedIsSmi(input), [=] { return __ FalseConstant(); },
      [=] {
        return __ SelectBooleanConstant(__ IsJSReceiver(__ CAST(input)));
      });
  return result;
}

TNode<Object> IntrinsicsGenerator::IsArray(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  TNode<Object> input = __ LoadRegisterFromRegisterList(args, 0);
  return IsInstanceType(input, JS_ARRAY_TYPE);
}

TNode<Object> IntrinsicsGenerator::IsSmi(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  TNode<Object> input = __ LoadRegisterFromRegisterList(args, 0);
  return __ SelectBooleanConstant(__ TaggedIsSmi(input));
}

TNode<Object> IntrinsicsGenerator::CopyDataProperties(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kCopyDataProperties,
                                arg_count);
}

TNode<Object> IntrinsicsGenerator::CreateIterResultObject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context,
                                Builtins::kCreateIterResultObject, arg_count);
}

TNode<Object> IntrinsicsGenerator::HasProperty(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kHasProperty,
                                arg_count);
}

TNode<Object> IntrinsicsGenerator::ToLength(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kToLength, arg_count);
}

TNode<Object> IntrinsicsGenerator::ToObject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kToObject, arg_count);
}

TNode<Object> IntrinsicsGenerator::Call(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
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
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  TNode<Object> sync_iterator = __ LoadRegisterFromRegisterList(args, 0);
  return __ CreateAsyncFromSyncIterator(context, sync_iterator);
}

TNode<Object> IntrinsicsGenerator::CreateJSGeneratorObject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kCreateGeneratorObject,
                                arg_count);
}

TNode<Object> IntrinsicsGenerator::GeneratorGetResumeMode(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  TNode<JSGeneratorObject> generator =
      __ CAST(__ LoadRegisterFromRegisterList(args, 0));
  const TNode<Object> value =
      __ LoadObjectField(generator, JSGeneratorObject::kResumeModeOffset);

  return value;
}

TNode<Object> IntrinsicsGenerator::GeneratorClose(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  TNode<JSGeneratorObject> generator =
      __ CAST(__ LoadRegisterFromRegisterList(args, 0));
  __ StoreObjectFieldNoWriteBarrier(
      generator, JSGeneratorObject::kContinuationOffset,
      __ SmiConstant(JSGeneratorObject::kGeneratorClosed));
  return __ UndefinedConstant();
}

TNode<Object> IntrinsicsGenerator::GetImportMetaObject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return __ GetImportMetaObject(context);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionAwaitCaught(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context,
                                Builtins::kAsyncFunctionAwaitCaught, arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionAwaitUncaught(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(
      args, context, Builtins::kAsyncFunctionAwaitUncaught, arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionEnter(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncFunctionEnter,
                                arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionReject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncFunctionReject,
                                arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncFunctionResolve(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncFunctionResolve,
                                arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorAwaitCaught(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(
      args, context, Builtins::kAsyncGeneratorAwaitCaught, arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorAwaitUncaught(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(
      args, context, Builtins::kAsyncGeneratorAwaitUncaught, arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorReject(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncGeneratorReject,
                                arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorResolve(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncGeneratorResolve,
                                arg_count);
}

TNode<Object> IntrinsicsGenerator::AsyncGeneratorYield(
    const InterpreterAssembler::RegListNodePair& args, TNode<Context> context,
    int arg_count) {
  return IntrinsicAsBuiltinCall(args, context, Builtins::kAsyncGeneratorYield,
                                arg_count);
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
