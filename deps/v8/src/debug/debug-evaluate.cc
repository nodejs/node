// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-evaluate.h"

#include "src/accessors.h"
#include "src/assembler-inl.h"
#include "src/compiler.h"
#include "src/contexts.h"
#include "src/debug/debug-frames.h"
#include "src/debug/debug-scopes.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/globals.h"
#include "src/interpreter/bytecode-array-iterator.h"
#include "src/interpreter/bytecodes.h"
#include "src/isolate-inl.h"
#include "src/snapshot/snapshot.h"

namespace v8 {
namespace internal {

MaybeHandle<Object> DebugEvaluate::Global(Isolate* isolate,
                                          Handle<String> source,
                                          bool throw_on_side_effect) {
  // Disable breaks in side-effect free mode.
  DisableBreak disable_break_scope(isolate->debug(), throw_on_side_effect);

  Handle<Context> context = isolate->native_context();
  ScriptOriginOptions origin_options(false, true);
  MaybeHandle<SharedFunctionInfo> maybe_function_info =
      Compiler::GetSharedFunctionInfoForScript(
          source, Compiler::ScriptDetails(isolate->factory()->empty_string()),
          origin_options, nullptr, nullptr, ScriptCompiler::kNoCompileOptions,
          ScriptCompiler::kNoCacheNoReason, NOT_NATIVES_CODE);

  Handle<SharedFunctionInfo> shared_info;
  if (!maybe_function_info.ToHandle(&shared_info)) return MaybeHandle<Object>();

  Handle<JSFunction> fun =
      isolate->factory()->NewFunctionFromSharedFunctionInfo(shared_info,
                                                            context);
  if (throw_on_side_effect) isolate->debug()->StartSideEffectCheckMode();
  MaybeHandle<Object> result = Execution::Call(
      isolate, fun, Handle<JSObject>(context->global_proxy()), 0, nullptr);
  if (throw_on_side_effect) isolate->debug()->StopSideEffectCheckMode();
  return result;
}

MaybeHandle<Object> DebugEvaluate::Local(Isolate* isolate,
                                         StackFrame::Id frame_id,
                                         int inlined_jsframe_index,
                                         Handle<String> source,
                                         bool throw_on_side_effect) {
  // Handle the processing of break.
  DisableBreak disable_break_scope(isolate->debug());

  // Get the frame where the debugging is performed.
  StackTraceFrameIterator it(isolate, frame_id);
  if (!it.is_javascript()) return isolate->factory()->undefined_value();
  JavaScriptFrame* frame = it.javascript_frame();

  // This is not a lot different than DebugEvaluate::Global, except that
  // variables accessible by the function we are evaluating from are
  // materialized and included on top of the native context. Changes to
  // the materialized object are written back afterwards.
  // Note that the native context is taken from the original context chain,
  // which may not be the current native context of the isolate.
  ContextBuilder context_builder(isolate, frame, inlined_jsframe_index);
  if (isolate->has_pending_exception()) return MaybeHandle<Object>();

  Handle<Context> context = context_builder.evaluation_context();
  Handle<JSObject> receiver(context->global_proxy());
  MaybeHandle<Object> maybe_result =
      Evaluate(isolate, context_builder.outer_info(), context, receiver, source,
               throw_on_side_effect);
  if (!maybe_result.is_null()) context_builder.UpdateValues();
  return maybe_result;
}

MaybeHandle<Object> DebugEvaluate::WithTopmostArguments(Isolate* isolate,
                                                        Handle<String> source) {
  // Handle the processing of break.
  DisableBreak disable_break_scope(isolate->debug());
  Factory* factory = isolate->factory();
  JavaScriptFrameIterator it(isolate);

  // Get context and receiver.
  Handle<Context> native_context(
      Context::cast(it.frame()->context())->native_context(), isolate);

  // Materialize arguments as property on an extension object.
  Handle<JSObject> materialized = factory->NewJSObjectWithNullProto();
  Handle<String> arguments_str = factory->arguments_string();
  JSObject::SetOwnPropertyIgnoreAttributes(
      materialized, arguments_str,
      Accessors::FunctionGetArguments(it.frame(), 0), NONE)
      .Check();

  // Materialize receiver.
  Handle<String> this_str = factory->this_string();
  JSObject::SetOwnPropertyIgnoreAttributes(
      materialized, this_str, Handle<Object>(it.frame()->receiver(), isolate),
      NONE)
      .Check();

  // Use extension object in a debug-evaluate scope.
  Handle<ScopeInfo> scope_info =
      ScopeInfo::CreateForWithScope(isolate, Handle<ScopeInfo>::null());
  scope_info->SetIsDebugEvaluateScope();
  Handle<Context> evaluation_context =
      factory->NewDebugEvaluateContext(native_context, scope_info, materialized,
                                       Handle<Context>(), Handle<StringSet>());
  Handle<SharedFunctionInfo> outer_info(native_context->closure()->shared(),
                                        isolate);
  Handle<JSObject> receiver(native_context->global_proxy());
  const bool throw_on_side_effect = false;
  MaybeHandle<Object> maybe_result =
      Evaluate(isolate, outer_info, evaluation_context, receiver, source,
               throw_on_side_effect);
  return maybe_result;
}

// Compile and evaluate source for the given context.
MaybeHandle<Object> DebugEvaluate::Evaluate(
    Isolate* isolate, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, Handle<Object> receiver, Handle<String> source,
    bool throw_on_side_effect) {
  Handle<JSFunction> eval_fun;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, eval_fun,
      Compiler::GetFunctionFromEval(source, outer_info, context,
                                    LanguageMode::kSloppy, NO_PARSE_RESTRICTION,
                                    kNoSourcePosition, kNoSourcePosition,
                                    kNoSourcePosition),
      Object);

  Handle<Object> result;
  bool sucess = false;
  if (throw_on_side_effect) isolate->debug()->StartSideEffectCheckMode();
  sucess = Execution::Call(isolate, eval_fun, receiver, 0, nullptr)
               .ToHandle(&result);
  if (throw_on_side_effect) isolate->debug()->StopSideEffectCheckMode();
  if (!sucess) {
    DCHECK(isolate->has_pending_exception());
    return MaybeHandle<Object>();
  }

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (result->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, Handle<JSGlobalProxy>::cast(result));
    // TODO(verwaest): This will crash when the global proxy is detached.
    result = PrototypeIterator::GetCurrent<JSObject>(iter);
  }

  return result;
}


DebugEvaluate::ContextBuilder::ContextBuilder(Isolate* isolate,
                                              JavaScriptFrame* frame,
                                              int inlined_jsframe_index)
    : isolate_(isolate),
      frame_(frame),
      inlined_jsframe_index_(inlined_jsframe_index) {
  FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);
  Handle<JSFunction> local_function = frame_inspector.GetFunction();
  Handle<Context> outer_context(local_function->context());
  evaluation_context_ = outer_context;
  outer_info_ = handle(local_function->shared());
  Factory* factory = isolate->factory();

  // To evaluate as if we were running eval at the point of the debug break,
  // we reconstruct the context chain as follows:
  //  - To make stack-allocated variables visible, we materialize them and
  //    use a debug-evaluate context to wrap both the materialized object and
  //    the original context.
  //  - We use the original context chain from the function context to the
  //    native context.
  //  - Between the function scope and the native context, we only resolve
  //    variable names that the current function already uses. Only for these
  //    names we can be sure that they will be correctly resolved. For the
  //    rest, we only resolve to with, script, and native contexts. We use a
  //    whitelist to implement that.
  // Context::Lookup has special handling for debug-evaluate contexts:
  //  - Look up in the materialized stack variables.
  //  - Look up in the original context.
  //  - Check the whitelist to find out whether to skip contexts during lookup.
  const ScopeIterator::Option option = ScopeIterator::COLLECT_NON_LOCALS;
  for (ScopeIterator it(isolate, &frame_inspector, option); !it.Done();
       it.Next()) {
    ScopeIterator::ScopeType scope_type = it.Type();
    if (scope_type == ScopeIterator::ScopeTypeLocal) {
      DCHECK_EQ(FUNCTION_SCOPE, it.CurrentScopeInfo()->scope_type());
      Handle<JSObject> materialized = factory->NewJSObjectWithNullProto();
      Handle<Context> local_context =
          it.HasContext() ? it.CurrentContext() : outer_context;
      Handle<StringSet> non_locals = it.GetNonLocals();
      MaterializeReceiver(materialized, local_context, local_function,
                          non_locals);
      MaterializeStackLocals(materialized, local_function, &frame_inspector);
      ContextChainElement context_chain_element;
      context_chain_element.scope_info = it.CurrentScopeInfo();
      context_chain_element.materialized_object = materialized;
      // Non-locals that are already being referenced by the current function
      // are guaranteed to be correctly resolved.
      context_chain_element.whitelist = non_locals;
      if (it.HasContext()) {
        context_chain_element.wrapped_context = it.CurrentContext();
      }
      context_chain_.push_back(context_chain_element);
      evaluation_context_ = outer_context;
      break;
    } else if (scope_type == ScopeIterator::ScopeTypeCatch ||
               scope_type == ScopeIterator::ScopeTypeWith ||
               scope_type == ScopeIterator::ScopeTypeModule) {
      ContextChainElement context_chain_element;
      Handle<Context> current_context = it.CurrentContext();
      if (!current_context->IsDebugEvaluateContext()) {
        context_chain_element.wrapped_context = current_context;
      }
      context_chain_.push_back(context_chain_element);
    } else if (scope_type == ScopeIterator::ScopeTypeBlock ||
               scope_type == ScopeIterator::ScopeTypeEval) {
      Handle<JSObject> materialized = factory->NewJSObjectWithNullProto();
      frame_inspector.MaterializeStackLocals(materialized,
                                             it.CurrentScopeInfo());
      ContextChainElement context_chain_element;
      context_chain_element.scope_info = it.CurrentScopeInfo();
      context_chain_element.materialized_object = materialized;
      if (it.HasContext()) {
        context_chain_element.wrapped_context = it.CurrentContext();
      }
      context_chain_.push_back(context_chain_element);
    } else {
      break;
    }
  }

  for (auto rit = context_chain_.rbegin(); rit != context_chain_.rend();
       rit++) {
    ContextChainElement element = *rit;
    Handle<ScopeInfo> scope_info(ScopeInfo::CreateForWithScope(
        isolate, evaluation_context_->IsNativeContext()
                     ? Handle<ScopeInfo>::null()
                     : Handle<ScopeInfo>(evaluation_context_->scope_info())));
    scope_info->SetIsDebugEvaluateScope();
    evaluation_context_ = factory->NewDebugEvaluateContext(
        evaluation_context_, scope_info, element.materialized_object,
        element.wrapped_context, element.whitelist);
  }
}


void DebugEvaluate::ContextBuilder::UpdateValues() {
  for (ContextChainElement& element : context_chain_) {
    if (!element.materialized_object.is_null()) {
      // Write back potential changes to materialized stack locals to the stack.
      FrameInspector(frame_, inlined_jsframe_index_, isolate_)
          .UpdateStackLocalsFromMaterializedObject(element.materialized_object,
                                                   element.scope_info);
    }
  }
}


void DebugEvaluate::ContextBuilder::MaterializeReceiver(
    Handle<JSObject> target, Handle<Context> local_context,
    Handle<JSFunction> local_function, Handle<StringSet> non_locals) {
  Handle<Object> recv = isolate_->factory()->undefined_value();
  Handle<String> name = isolate_->factory()->this_string();
  if (non_locals->Has(name)) {
    // 'this' is allocated in an outer context and is is already being
    // referenced by the current function, so it can be correctly resolved.
    return;
  } else if (local_function->shared()->scope_info()->HasReceiver() &&
             !frame_->receiver()->IsTheHole(isolate_)) {
    recv = handle(frame_->receiver(), isolate_);
  }
  JSObject::SetOwnPropertyIgnoreAttributes(target, name, recv, NONE).Check();
}

void DebugEvaluate::ContextBuilder::MaterializeStackLocals(
    Handle<JSObject> target, Handle<JSFunction> function,
    FrameInspector* frame_inspector) {
  bool materialize_arguments_object = true;

  // Do not materialize the arguments object for eval or top-level code.
  if (function->shared()->is_toplevel()) materialize_arguments_object = false;

  // First materialize stack locals (modulo arguments object).
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());
  frame_inspector->MaterializeStackLocals(target, scope_info,
                                          materialize_arguments_object);

  // Then materialize the arguments object.
  if (materialize_arguments_object) {
    // Skip if "arguments" is already taken and wasn't optimized out (which
    // causes {MaterializeStackLocals} above to skip the local variable).
    Handle<String> arguments_str = isolate_->factory()->arguments_string();
    Maybe<bool> maybe = JSReceiver::HasOwnProperty(target, arguments_str);
    DCHECK(maybe.IsJust());
    if (maybe.FromJust()) return;

    // FunctionGetArguments can't throw an exception.
    Handle<JSObject> arguments =
        Accessors::FunctionGetArguments(frame_, inlined_jsframe_index_);
    JSObject::SetOwnPropertyIgnoreAttributes(target, arguments_str, arguments,
                                             NONE)
        .Check();
  }
}

namespace {

bool IntrinsicHasNoSideEffect(Runtime::FunctionId id) {
// Use macro to include both inlined and non-inlined version of an intrinsic.
#define INTRINSIC_WHITELIST(V)           \
  /* Conversions */                      \
  V(NumberToStringSkipCache)             \
  V(ToBigInt)                            \
  V(ToInteger)                           \
  V(ToLength)                            \
  V(ToNumber)                            \
  V(ToObject)                            \
  V(ToString)                            \
  /* Type checks */                      \
  V(IsArray)                             \
  V(IsDate)                              \
  V(IsFunction)                          \
  V(IsJSMap)                             \
  V(IsJSProxy)                           \
  V(IsJSReceiver)                        \
  V(IsJSSet)                             \
  V(IsJSWeakMap)                         \
  V(IsJSWeakSet)                         \
  V(IsRegExp)                            \
  V(IsSmi)                               \
  V(IsTypedArray)                        \
  /* Loads */                            \
  V(LoadLookupSlotForCall)               \
  /* Arrays */                           \
  V(ArraySpeciesConstructor)             \
  V(EstimateNumberOfElements)            \
  V(GetArrayKeys)                        \
  V(HasComplexElements)                  \
  V(NewArray)                            \
  V(NormalizeElements)                   \
  V(TrySliceSimpleNonFastElements)       \
  V(TypedArrayGetBuffer)                 \
  /* Errors */                           \
  V(NewTypeError)                        \
  V(ReThrow)                             \
  V(ThrowCalledNonCallable)              \
  V(ThrowInvalidStringLength)            \
  V(ThrowIteratorResultNotAnObject)      \
  V(ThrowReferenceError)                 \
  V(ThrowSymbolIteratorInvalid)          \
  /* Strings */                          \
  V(RegExpInternalReplace)               \
  V(StringIncludes)                      \
  V(StringIndexOf)                       \
  V(StringReplaceOneCharWithString)      \
  V(StringSubstring)                     \
  V(StringToNumber)                      \
  V(StringTrim)                          \
  /* BigInts */                          \
  V(BigIntEqualToBigInt)                 \
  V(BigIntToBoolean)                     \
  V(BigIntToNumber)                      \
  /* Literals */                         \
  V(CreateArrayLiteral)                  \
  V(CreateObjectLiteral)                 \
  V(CreateRegExpLiteral)                 \
  /* Called from builtins */             \
  V(AllocateInNewSpace)                  \
  V(AllocateInTargetSpace)               \
  V(AllocateSeqOneByteString)            \
  V(AllocateSeqTwoByteString)            \
  V(ArrayIncludes_Slow)                  \
  V(ArrayIndexOf)                        \
  V(ArrayIsArray)                        \
  V(ClassOf)                             \
  V(GenerateRandomNumbers)               \
  V(GetFunctionName)                     \
  V(GetOwnPropertyDescriptor)            \
  V(GlobalPrint)                         \
  V(HasProperty)                         \
  V(ObjectCreate)                        \
  V(ObjectEntries)                       \
  V(ObjectEntriesSkipFastPath)           \
  V(ObjectHasOwnProperty)                \
  V(ObjectValues)                        \
  V(ObjectValuesSkipFastPath)            \
  V(RegExpInitializeAndCompile)          \
  V(StackGuard)                          \
  V(StringAdd)                           \
  V(StringCharCodeAt)                    \
  V(StringEqual)                         \
  V(StringIndexOfUnchecked)              \
  V(StringParseFloat)                    \
  V(StringParseInt)                      \
  V(SymbolDescriptiveString)             \
  V(ThrowRangeError)                     \
  V(ThrowTypeError)                      \
  V(ToName)                              \
  /* Misc. */                            \
  V(Call)                                \
  V(CompleteInobjectSlackTrackingForMap) \
  V(HasInPrototypeChain)                 \
  V(MaxSmi)                              \
  V(NewObject)                           \
  V(StringMaxLength)                     \
  /* Test */                             \
  V(GetOptimizationStatus)               \
  V(OptimizeFunctionOnNextCall)          \
  V(OptimizeOsr)                         \
  V(UnblockConcurrentRecompilation)

#define CASE(Name)       \
  case Runtime::k##Name: \
  case Runtime::kInline##Name:

  switch (id) {
    INTRINSIC_WHITELIST(CASE)
    return true;
    default:
      if (FLAG_trace_side_effect_free_debug_evaluate) {
        PrintF("[debug-evaluate] intrinsic %s may cause side effect.\n",
               Runtime::FunctionForId(id)->name);
      }
      return false;
  }

#undef CASE
#undef INTRINSIC_WHITELIST
}

#ifdef DEBUG
bool BuiltinToIntrinsicHasNoSideEffect(Builtins::Name builtin_id,
                                       Runtime::FunctionId intrinsic_id) {
  // First check the intrinsic whitelist.
  if (IntrinsicHasNoSideEffect(intrinsic_id)) return true;

// Whitelist intrinsics called from specific builtins.
#define BUILTIN_INTRINSIC_WHITELIST(V, W)                                 \
  /* Arrays */                                                            \
  V(Builtins::kArrayFilter, W(CreateDataProperty))                        \
  V(Builtins::kArrayMap, W(CreateDataProperty))                           \
  V(Builtins::kArrayPrototypeSlice, W(CreateDataProperty) W(SetProperty)) \
  /* TypedArrays */                                                       \
  V(Builtins::kTypedArrayConstructor,                                     \
    W(TypedArrayCopyElements) W(ThrowInvalidTypedArrayAlignment))         \
  V(Builtins::kTypedArrayPrototypeFilter, W(TypedArrayCopyElements))      \
  V(Builtins::kTypedArrayPrototypeMap, W(SetProperty))

#define CASE(Builtin, ...) \
  case Builtin:            \
    return (__VA_ARGS__ false);

#define MATCH(Intrinsic)                   \
  intrinsic_id == Runtime::k##Intrinsic || \
      intrinsic_id == Runtime::kInline##Intrinsic ||

  switch (builtin_id) {
    BUILTIN_INTRINSIC_WHITELIST(CASE, MATCH)
    default:
      return false;
  }

#undef MATCH
#undef CASE
#undef BUILTIN_INTRINSIC_WHITELIST
}
#endif  // DEBUG

bool BytecodeHasNoSideEffect(interpreter::Bytecode bytecode) {
  typedef interpreter::Bytecode Bytecode;
  typedef interpreter::Bytecodes Bytecodes;
  if (Bytecodes::IsWithoutExternalSideEffects(bytecode)) return true;
  if (Bytecodes::IsCallOrConstruct(bytecode)) return true;
  if (Bytecodes::IsJumpIfToBoolean(bytecode)) return true;
  if (Bytecodes::IsPrefixScalingBytecode(bytecode)) return true;
  switch (bytecode) {
    // Whitelist for bytecodes.
    // Loads.
    case Bytecode::kLdaLookupSlot:
    case Bytecode::kLdaGlobal:
    case Bytecode::kLdaNamedProperty:
    case Bytecode::kLdaKeyedProperty:
    // Arithmetics.
    case Bytecode::kAdd:
    case Bytecode::kAddSmi:
    case Bytecode::kSub:
    case Bytecode::kSubSmi:
    case Bytecode::kMul:
    case Bytecode::kMulSmi:
    case Bytecode::kDiv:
    case Bytecode::kDivSmi:
    case Bytecode::kMod:
    case Bytecode::kModSmi:
    case Bytecode::kExp:
    case Bytecode::kExpSmi:
    case Bytecode::kNegate:
    case Bytecode::kBitwiseAnd:
    case Bytecode::kBitwiseAndSmi:
    case Bytecode::kBitwiseNot:
    case Bytecode::kBitwiseOr:
    case Bytecode::kBitwiseOrSmi:
    case Bytecode::kBitwiseXor:
    case Bytecode::kBitwiseXorSmi:
    case Bytecode::kShiftLeft:
    case Bytecode::kShiftLeftSmi:
    case Bytecode::kShiftRight:
    case Bytecode::kShiftRightSmi:
    case Bytecode::kShiftRightLogical:
    case Bytecode::kShiftRightLogicalSmi:
    case Bytecode::kInc:
    case Bytecode::kDec:
    case Bytecode::kLogicalNot:
    case Bytecode::kToBooleanLogicalNot:
    case Bytecode::kTypeOf:
    // Contexts.
    case Bytecode::kCreateBlockContext:
    case Bytecode::kCreateCatchContext:
    case Bytecode::kCreateFunctionContext:
    case Bytecode::kCreateEvalContext:
    case Bytecode::kCreateWithContext:
    // Literals.
    case Bytecode::kCreateArrayLiteral:
    case Bytecode::kCreateEmptyArrayLiteral:
    case Bytecode::kCreateObjectLiteral:
    case Bytecode::kCreateEmptyObjectLiteral:
    case Bytecode::kCreateRegExpLiteral:
    // Allocations.
    case Bytecode::kCreateClosure:
    case Bytecode::kCreateUnmappedArguments:
    case Bytecode::kCreateRestParameter:
    // Comparisons.
    case Bytecode::kTestEqual:
    case Bytecode::kTestEqualStrict:
    case Bytecode::kTestLessThan:
    case Bytecode::kTestLessThanOrEqual:
    case Bytecode::kTestGreaterThan:
    case Bytecode::kTestGreaterThanOrEqual:
    case Bytecode::kTestInstanceOf:
    case Bytecode::kTestIn:
    case Bytecode::kTestEqualStrictNoFeedback:
    case Bytecode::kTestUndetectable:
    case Bytecode::kTestTypeOf:
    case Bytecode::kTestUndefined:
    case Bytecode::kTestNull:
    // Conversions.
    case Bytecode::kToObject:
    case Bytecode::kToNumber:
    case Bytecode::kToName:
    case Bytecode::kToString:
    // Misc.
    case Bytecode::kForInEnumerate:
    case Bytecode::kForInPrepare:
    case Bytecode::kForInContinue:
    case Bytecode::kForInNext:
    case Bytecode::kForInStep:
    case Bytecode::kThrow:
    case Bytecode::kReThrow:
    case Bytecode::kThrowReferenceErrorIfHole:
    case Bytecode::kThrowSuperNotCalledIfHole:
    case Bytecode::kThrowSuperAlreadyCalledIfNotHole:
    case Bytecode::kIllegal:
    case Bytecode::kCallJSRuntime:
    case Bytecode::kStackCheck:
    case Bytecode::kReturn:
    case Bytecode::kSetPendingMessage:
      return true;
    default:
      return false;
  }
}

bool BuiltinHasNoSideEffect(Builtins::Name id) {
  switch (id) {
    // Whitelist for builtins.
    // Object builtins.
    case Builtins::kObjectConstructor:
    case Builtins::kObjectCreate:
    case Builtins::kObjectEntries:
    case Builtins::kObjectGetOwnPropertyDescriptor:
    case Builtins::kObjectGetOwnPropertyDescriptors:
    case Builtins::kObjectGetOwnPropertyNames:
    case Builtins::kObjectGetOwnPropertySymbols:
    case Builtins::kObjectGetPrototypeOf:
    case Builtins::kObjectIs:
    case Builtins::kObjectIsExtensible:
    case Builtins::kObjectIsFrozen:
    case Builtins::kObjectIsSealed:
    case Builtins::kObjectPrototypeValueOf:
    case Builtins::kObjectValues:
    case Builtins::kObjectPrototypeHasOwnProperty:
    case Builtins::kObjectPrototypeIsPrototypeOf:
    case Builtins::kObjectPrototypePropertyIsEnumerable:
    case Builtins::kObjectPrototypeToString:
    // Array builtins.
    case Builtins::kArrayIsArray:
    case Builtins::kArrayConstructor:
    case Builtins::kArrayIndexOf:
    case Builtins::kArrayPrototypeValues:
    case Builtins::kArrayIncludes:
    case Builtins::kArrayPrototypeEntries:
    case Builtins::kArrayPrototypeFind:
    case Builtins::kArrayPrototypeFindIndex:
    case Builtins::kArrayPrototypeKeys:
    case Builtins::kArrayPrototypeSlice:
    case Builtins::kArrayForEach:
    case Builtins::kArrayEvery:
    case Builtins::kArraySome:
    case Builtins::kArrayConcat:
    case Builtins::kArrayFilter:
    case Builtins::kArrayMap:
    case Builtins::kArrayReduce:
    case Builtins::kArrayReduceRight:
    // TypedArray builtins.
    case Builtins::kTypedArrayConstructor:
    case Builtins::kTypedArrayPrototypeBuffer:
    case Builtins::kTypedArrayPrototypeByteLength:
    case Builtins::kTypedArrayPrototypeByteOffset:
    case Builtins::kTypedArrayPrototypeLength:
    case Builtins::kTypedArrayPrototypeEntries:
    case Builtins::kTypedArrayPrototypeKeys:
    case Builtins::kTypedArrayPrototypeValues:
    case Builtins::kTypedArrayPrototypeFind:
    case Builtins::kTypedArrayPrototypeFindIndex:
    case Builtins::kTypedArrayPrototypeIncludes:
    case Builtins::kTypedArrayPrototypeIndexOf:
    case Builtins::kTypedArrayPrototypeLastIndexOf:
    case Builtins::kTypedArrayPrototypeSlice:
    case Builtins::kTypedArrayPrototypeSubArray:
    case Builtins::kTypedArrayPrototypeEvery:
    case Builtins::kTypedArrayPrototypeSome:
    case Builtins::kTypedArrayPrototypeFilter:
    case Builtins::kTypedArrayPrototypeMap:
    case Builtins::kTypedArrayPrototypeReduce:
    case Builtins::kTypedArrayPrototypeReduceRight:
    case Builtins::kTypedArrayPrototypeForEach:
    // ArrayBuffer builtins.
    case Builtins::kArrayBufferConstructor:
    case Builtins::kArrayBufferPrototypeGetByteLength:
    case Builtins::kArrayBufferIsView:
    case Builtins::kArrayBufferPrototypeSlice:
    case Builtins::kReturnReceiver:
    // DataView builtins.
    case Builtins::kDataViewConstructor:
    case Builtins::kDataViewPrototypeGetBuffer:
    case Builtins::kDataViewPrototypeGetByteLength:
    case Builtins::kDataViewPrototypeGetByteOffset:
    case Builtins::kDataViewPrototypeGetInt8:
    case Builtins::kDataViewPrototypeGetUint8:
    case Builtins::kDataViewPrototypeGetInt16:
    case Builtins::kDataViewPrototypeGetUint16:
    case Builtins::kDataViewPrototypeGetInt32:
    case Builtins::kDataViewPrototypeGetUint32:
    case Builtins::kDataViewPrototypeGetFloat32:
    case Builtins::kDataViewPrototypeGetFloat64:
    case Builtins::kDataViewPrototypeGetBigInt64:
    case Builtins::kDataViewPrototypeGetBigUint64:
    // Boolean bulitins.
    case Builtins::kBooleanConstructor:
    case Builtins::kBooleanPrototypeToString:
    case Builtins::kBooleanPrototypeValueOf:
    // Date builtins.
    case Builtins::kDateConstructor:
    case Builtins::kDateNow:
    case Builtins::kDateParse:
    case Builtins::kDatePrototypeGetDate:
    case Builtins::kDatePrototypeGetDay:
    case Builtins::kDatePrototypeGetFullYear:
    case Builtins::kDatePrototypeGetHours:
    case Builtins::kDatePrototypeGetMilliseconds:
    case Builtins::kDatePrototypeGetMinutes:
    case Builtins::kDatePrototypeGetMonth:
    case Builtins::kDatePrototypeGetSeconds:
    case Builtins::kDatePrototypeGetTime:
    case Builtins::kDatePrototypeGetTimezoneOffset:
    case Builtins::kDatePrototypeGetUTCDate:
    case Builtins::kDatePrototypeGetUTCDay:
    case Builtins::kDatePrototypeGetUTCFullYear:
    case Builtins::kDatePrototypeGetUTCHours:
    case Builtins::kDatePrototypeGetUTCMilliseconds:
    case Builtins::kDatePrototypeGetUTCMinutes:
    case Builtins::kDatePrototypeGetUTCMonth:
    case Builtins::kDatePrototypeGetUTCSeconds:
    case Builtins::kDatePrototypeGetYear:
    case Builtins::kDatePrototypeToDateString:
    case Builtins::kDatePrototypeToISOString:
    case Builtins::kDatePrototypeToUTCString:
    case Builtins::kDatePrototypeToString:
    case Builtins::kDatePrototypeToTimeString:
    case Builtins::kDatePrototypeToJson:
    case Builtins::kDatePrototypeToPrimitive:
    case Builtins::kDatePrototypeValueOf:
    // Map builtins.
    case Builtins::kMapConstructor:
    case Builtins::kMapPrototypeForEach:
    case Builtins::kMapPrototypeGet:
    case Builtins::kMapPrototypeHas:
    case Builtins::kMapPrototypeEntries:
    case Builtins::kMapPrototypeGetSize:
    case Builtins::kMapPrototypeKeys:
    case Builtins::kMapPrototypeValues:
    // WeakMap builtins.
    case Builtins::kWeakMapConstructor:
    case Builtins::kWeakMapGet:
    case Builtins::kWeakMapHas:
    // Math builtins.
    case Builtins::kMathAbs:
    case Builtins::kMathAcos:
    case Builtins::kMathAcosh:
    case Builtins::kMathAsin:
    case Builtins::kMathAsinh:
    case Builtins::kMathAtan:
    case Builtins::kMathAtanh:
    case Builtins::kMathAtan2:
    case Builtins::kMathCeil:
    case Builtins::kMathCbrt:
    case Builtins::kMathExpm1:
    case Builtins::kMathClz32:
    case Builtins::kMathCos:
    case Builtins::kMathCosh:
    case Builtins::kMathExp:
    case Builtins::kMathFloor:
    case Builtins::kMathFround:
    case Builtins::kMathHypot:
    case Builtins::kMathImul:
    case Builtins::kMathLog:
    case Builtins::kMathLog1p:
    case Builtins::kMathLog2:
    case Builtins::kMathLog10:
    case Builtins::kMathMax:
    case Builtins::kMathMin:
    case Builtins::kMathPow:
    case Builtins::kMathRandom:
    case Builtins::kMathRound:
    case Builtins::kMathSign:
    case Builtins::kMathSin:
    case Builtins::kMathSinh:
    case Builtins::kMathSqrt:
    case Builtins::kMathTan:
    case Builtins::kMathTanh:
    case Builtins::kMathTrunc:
    // Number builtins.
    case Builtins::kNumberConstructor:
    case Builtins::kNumberIsFinite:
    case Builtins::kNumberIsInteger:
    case Builtins::kNumberIsNaN:
    case Builtins::kNumberIsSafeInteger:
    case Builtins::kNumberParseFloat:
    case Builtins::kNumberParseInt:
    case Builtins::kNumberPrototypeToExponential:
    case Builtins::kNumberPrototypeToFixed:
    case Builtins::kNumberPrototypeToPrecision:
    case Builtins::kNumberPrototypeToString:
    case Builtins::kNumberPrototypeValueOf:
    // BigInt builtins.
    case Builtins::kBigIntConstructor:
    case Builtins::kBigIntAsIntN:
    case Builtins::kBigIntAsUintN:
    case Builtins::kBigIntPrototypeToString:
    case Builtins::kBigIntPrototypeValueOf:
    // Set builtins.
    case Builtins::kSetConstructor:
    case Builtins::kSetPrototypeEntries:
    case Builtins::kSetPrototypeForEach:
    case Builtins::kSetPrototypeGetSize:
    case Builtins::kSetPrototypeHas:
    case Builtins::kSetPrototypeValues:
    // WeakSet builtins.
    case Builtins::kWeakSetConstructor:
    case Builtins::kWeakSetHas:
    // String builtins. Strings are immutable.
    case Builtins::kStringFromCharCode:
    case Builtins::kStringFromCodePoint:
    case Builtins::kStringConstructor:
    case Builtins::kStringPrototypeAnchor:
    case Builtins::kStringPrototypeBig:
    case Builtins::kStringPrototypeBlink:
    case Builtins::kStringPrototypeBold:
    case Builtins::kStringPrototypeCharAt:
    case Builtins::kStringPrototypeCharCodeAt:
    case Builtins::kStringPrototypeCodePointAt:
    case Builtins::kStringPrototypeConcat:
    case Builtins::kStringPrototypeEndsWith:
    case Builtins::kStringPrototypeFixed:
    case Builtins::kStringPrototypeFontcolor:
    case Builtins::kStringPrototypeFontsize:
    case Builtins::kStringPrototypeIncludes:
    case Builtins::kStringPrototypeIndexOf:
    case Builtins::kStringPrototypeItalics:
    case Builtins::kStringPrototypeLastIndexOf:
    case Builtins::kStringPrototypeLink:
    case Builtins::kStringPrototypePadEnd:
    case Builtins::kStringPrototypePadStart:
    case Builtins::kStringPrototypeRepeat:
    case Builtins::kStringPrototypeSlice:
    case Builtins::kStringPrototypeSmall:
    case Builtins::kStringPrototypeStartsWith:
    case Builtins::kStringPrototypeStrike:
    case Builtins::kStringPrototypeSub:
    case Builtins::kStringPrototypeSubstr:
    case Builtins::kStringPrototypeSubstring:
    case Builtins::kStringPrototypeSup:
    case Builtins::kStringPrototypeToString:
#ifndef V8_INTL_SUPPORT
    case Builtins::kStringPrototypeToLowerCase:
    case Builtins::kStringPrototypeToUpperCase:
#endif
    case Builtins::kStringPrototypeTrim:
    case Builtins::kStringPrototypeTrimEnd:
    case Builtins::kStringPrototypeTrimStart:
    case Builtins::kStringPrototypeValueOf:
    case Builtins::kStringToNumber:
    case Builtins::kStringSubstring:
    // Symbol builtins.
    case Builtins::kSymbolConstructor:
    case Builtins::kSymbolKeyFor:
    case Builtins::kSymbolPrototypeToString:
    case Builtins::kSymbolPrototypeValueOf:
    case Builtins::kSymbolPrototypeToPrimitive:
    // JSON builtins.
    case Builtins::kJsonParse:
    case Builtins::kJsonStringify:
    // Global function builtins.
    case Builtins::kGlobalDecodeURI:
    case Builtins::kGlobalDecodeURIComponent:
    case Builtins::kGlobalEncodeURI:
    case Builtins::kGlobalEncodeURIComponent:
    case Builtins::kGlobalEscape:
    case Builtins::kGlobalUnescape:
    case Builtins::kGlobalIsFinite:
    case Builtins::kGlobalIsNaN:
    // Function builtins.
    case Builtins::kFunctionPrototypeToString:
    case Builtins::kFunctionPrototypeBind:
    case Builtins::kFastFunctionPrototypeBind:
    case Builtins::kFunctionPrototypeCall:
    case Builtins::kFunctionPrototypeApply:
    // Error builtins.
    case Builtins::kErrorConstructor:
    case Builtins::kMakeError:
    case Builtins::kMakeTypeError:
    case Builtins::kMakeSyntaxError:
    case Builtins::kMakeRangeError:
    case Builtins::kMakeURIError:
      return true;
    default:
      if (FLAG_trace_side_effect_free_debug_evaluate) {
        PrintF("[debug-evaluate] built-in %s may cause side effect.\n",
               Builtins::name(id));
      }
      return false;
  }
}

bool BytecodeRequiresRuntimeCheck(interpreter::Bytecode bytecode) {
  typedef interpreter::Bytecode Bytecode;
  switch (bytecode) {
    case Bytecode::kStaNamedProperty:
    case Bytecode::kStaNamedOwnProperty:
    case Bytecode::kStaKeyedProperty:
    case Bytecode::kStaInArrayLiteral:
    case Bytecode::kStaDataPropertyInLiteral:
    case Bytecode::kStaCurrentContextSlot:
      return true;
    default:
      return false;
  }
}

}  // anonymous namespace

// static
DebugEvaluate::SideEffectState DebugEvaluate::FunctionGetSideEffectState(
    Handle<SharedFunctionInfo> info) {
  if (FLAG_trace_side_effect_free_debug_evaluate) {
    PrintF("[debug-evaluate] Checking function %s for side effect.\n",
           info->DebugName()->ToCString().get());
  }

  DCHECK(info->is_compiled());
  if (info->HasBytecodeArray()) {
    // Check bytecodes against whitelist.
    Handle<BytecodeArray> bytecode_array(info->GetBytecodeArray());
    if (FLAG_trace_side_effect_free_debug_evaluate) bytecode_array->Print();
    bool requires_runtime_checks = false;
    for (interpreter::BytecodeArrayIterator it(bytecode_array); !it.done();
         it.Advance()) {
      interpreter::Bytecode bytecode = it.current_bytecode();

      if (interpreter::Bytecodes::IsCallRuntime(bytecode)) {
        Runtime::FunctionId id =
            (bytecode == interpreter::Bytecode::kInvokeIntrinsic)
                ? it.GetIntrinsicIdOperand(0)
                : it.GetRuntimeIdOperand(0);
        if (IntrinsicHasNoSideEffect(id)) continue;
        return kHasSideEffects;
      }

      if (BytecodeHasNoSideEffect(bytecode)) continue;
      if (BytecodeRequiresRuntimeCheck(bytecode)) {
        requires_runtime_checks = true;
        continue;
      }

      if (FLAG_trace_side_effect_free_debug_evaluate) {
        PrintF("[debug-evaluate] bytecode %s may cause side effect.\n",
               interpreter::Bytecodes::ToString(bytecode));
      }

      // Did not match whitelist.
      return kHasSideEffects;
    }
    return requires_runtime_checks ? kRequiresRuntimeChecks : kHasNoSideEffect;
  } else if (info->IsApiFunction()) {
    if (info->GetCode()->is_builtin()) {
      return info->GetCode()->builtin_index() == Builtins::kHandleApiCall
                 ? kHasNoSideEffect
                 : kHasSideEffects;
    }
  } else {
    // Check built-ins against whitelist.
    int builtin_index =
        info->HasBuiltinId() ? info->builtin_id() : Builtins::kNoBuiltinId;
    DCHECK_NE(Builtins::kDeserializeLazy, builtin_index);
    if (Builtins::IsBuiltinId(builtin_index) &&
        BuiltinHasNoSideEffect(static_cast<Builtins::Name>(builtin_index))) {
#ifdef DEBUG
      Isolate* isolate = info->GetIsolate();
      Code* code = isolate->builtins()->builtin(builtin_index);
      if (code->builtin_index() == Builtins::kDeserializeLazy) {
        // Target builtin is not yet deserialized. Deserialize it now.

        DCHECK(Builtins::IsLazy(builtin_index));
        DCHECK_EQ(Builtins::TFJ, Builtins::KindOf(builtin_index));

        code = Snapshot::DeserializeBuiltin(isolate, builtin_index);
        DCHECK_NE(Builtins::kDeserializeLazy, code->builtin_index());
      }
      // TODO(yangguo): Check builtin-to-builtin calls too.
      int mode = RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE);
      bool failed = false;
      for (RelocIterator it(code, mode); !it.done(); it.next()) {
        RelocInfo* rinfo = it.rinfo();
        Address address = rinfo->target_external_reference();
        const Runtime::Function* function = Runtime::FunctionForEntry(address);
        if (function == nullptr) continue;
        if (!BuiltinToIntrinsicHasNoSideEffect(
                static_cast<Builtins::Name>(builtin_index),
                function->function_id)) {
          PrintF("Whitelisted builtin %s calls non-whitelisted intrinsic %s\n",
                 Builtins::name(builtin_index), function->name);
          failed = true;
        }
        DCHECK(!failed);
      }
#endif  // DEBUG
      return kHasNoSideEffect;
    }
  }

  return kHasSideEffects;
}

// static
bool DebugEvaluate::CallbackHasNoSideEffect(Object* callback_info) {
  DisallowHeapAllocation no_gc;
  if (callback_info->IsAccessorInfo()) {
    // List of whitelisted internal accessors can be found in accessors.h.
    AccessorInfo* info = AccessorInfo::cast(callback_info);
    if (info->has_no_side_effect()) return true;
    if (FLAG_trace_side_effect_free_debug_evaluate) {
      PrintF("[debug-evaluate] API Callback '");
      info->name()->ShortPrint();
      PrintF("' may cause side effect.\n");
    }
  } else if (callback_info->IsInterceptorInfo()) {
    InterceptorInfo* info = InterceptorInfo::cast(callback_info);
    if (info->has_no_side_effect()) return true;
    if (FLAG_trace_side_effect_free_debug_evaluate) {
      PrintF("[debug-evaluate] API Interceptor may cause side effect.\n");
    }
  } else if (callback_info->IsCallHandlerInfo()) {
    CallHandlerInfo* info = CallHandlerInfo::cast(callback_info);
    if (info->IsSideEffectFreeCallHandlerInfo()) return true;
    if (FLAG_trace_side_effect_free_debug_evaluate) {
      PrintF("[debug-evaluate] API CallHandlerInfo may cause side effect.\n");
    }
  }
  return false;
}

// static
void DebugEvaluate::ApplySideEffectChecks(
    Handle<BytecodeArray> bytecode_array) {
  for (interpreter::BytecodeArrayIterator it(bytecode_array); !it.done();
       it.Advance()) {
    interpreter::Bytecode bytecode = it.current_bytecode();
    if (BytecodeRequiresRuntimeCheck(bytecode)) {
      interpreter::Bytecode debugbreak =
          interpreter::Bytecodes::GetDebugBreak(bytecode);
      bytecode_array->set(it.current_offset(),
                          interpreter::Bytecodes::ToByte(debugbreak));
    }
  }
}

}  // namespace internal
}  // namespace v8
