// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-evaluate.h"

#include "src/accessors.h"
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

namespace v8 {
namespace internal {

static inline bool IsDebugContext(Isolate* isolate, Context* context) {
  return context->native_context() == *isolate->debug()->debug_context();
}

MaybeHandle<Object> DebugEvaluate::Global(Isolate* isolate,
                                          Handle<String> source) {
  // Handle the processing of break.
  DisableBreak disable_break_scope(isolate->debug());

  // Enter the top context from before the debugger was invoked.
  SaveContext save(isolate);
  SaveContext* top = &save;
  while (top != NULL && IsDebugContext(isolate, *top->context())) {
    top = top->prev();
  }
  if (top != NULL) isolate->set_context(*top->context());

  // Get the native context now set to the top context from before the
  // debugger was invoked.
  Handle<Context> context = isolate->native_context();
  Handle<JSObject> receiver(context->global_proxy());
  Handle<SharedFunctionInfo> outer_info(context->closure()->shared(), isolate);
  return Evaluate(isolate, outer_info, context, receiver, source, false);
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

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save =
      DebugFrameHelper::FindSavedContextForFrame(isolate, frame);
  SaveContext savex(isolate);
  isolate->set_context(*(save->context()));

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


// Compile and evaluate source for the given context.
MaybeHandle<Object> DebugEvaluate::Evaluate(
    Isolate* isolate, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, Handle<Object> receiver, Handle<String> source,
    bool throw_on_side_effect) {
  Handle<JSFunction> eval_fun;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, eval_fun,
      Compiler::GetFunctionFromEval(source, outer_info, context, SLOPPY,
                                    NO_PARSE_RESTRICTION, kNoSourcePosition,
                                    kNoSourcePosition, kNoSourcePosition),
      Object);

  Handle<Object> result;
  {
    NoSideEffectScope no_side_effect(isolate, throw_on_side_effect);
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, result, Execution::Call(isolate, eval_fun, receiver, 0, NULL),
        Object);
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
      frame_inspector.MaterializeStackLocals(materialized, local_function);
      MaterializeArgumentsObject(materialized, local_function);
      ContextChainElement context_chain_element;
      context_chain_element.scope_info = it.CurrentScopeInfo();
      context_chain_element.materialized_object = materialized;
      // Non-locals that are already being referenced by the current function
      // are guaranteed to be correctly resolved.
      context_chain_element.whitelist = non_locals;
      if (it.HasContext()) {
        context_chain_element.wrapped_context = it.CurrentContext();
      }
      context_chain_.Add(context_chain_element);
      evaluation_context_ = outer_context;
      break;
    } else if (scope_type == ScopeIterator::ScopeTypeCatch ||
               scope_type == ScopeIterator::ScopeTypeWith) {
      ContextChainElement context_chain_element;
      Handle<Context> current_context = it.CurrentContext();
      if (!current_context->IsDebugEvaluateContext()) {
        context_chain_element.wrapped_context = current_context;
      }
      context_chain_.Add(context_chain_element);
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
      context_chain_.Add(context_chain_element);
    } else {
      break;
    }
  }

  for (int i = context_chain_.length() - 1; i >= 0; i--) {
    Handle<ScopeInfo> scope_info(ScopeInfo::CreateForWithScope(
        isolate, evaluation_context_->IsNativeContext()
                     ? Handle<ScopeInfo>::null()
                     : Handle<ScopeInfo>(evaluation_context_->scope_info())));
    scope_info->SetIsDebugEvaluateScope();
    evaluation_context_ = factory->NewDebugEvaluateContext(
        evaluation_context_, scope_info, context_chain_[i].materialized_object,
        context_chain_[i].wrapped_context, context_chain_[i].whitelist);
  }
}


void DebugEvaluate::ContextBuilder::UpdateValues() {
  for (int i = 0; i < context_chain_.length(); i++) {
    ContextChainElement element = context_chain_[i];
    if (!element.materialized_object.is_null()) {
      // Write back potential changes to materialized stack locals to the stack.
      FrameInspector(frame_, inlined_jsframe_index_, isolate_)
          .UpdateStackLocalsFromMaterializedObject(element.materialized_object,
                                                   element.scope_info);
    }
  }
}


void DebugEvaluate::ContextBuilder::MaterializeArgumentsObject(
    Handle<JSObject> target, Handle<JSFunction> function) {
  // Do not materialize the arguments object for eval or top-level code.
  // Skip if "arguments" is already taken.
  if (function->shared()->is_toplevel()) return;
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(
      target, isolate_->factory()->arguments_string());
  DCHECK(maybe.IsJust());
  if (maybe.FromJust()) return;

  // FunctionGetArguments can't throw an exception.
  Handle<JSObject> arguments = Accessors::FunctionGetArguments(function);
  Handle<String> arguments_str = isolate_->factory()->arguments_string();
  JSObject::SetOwnPropertyIgnoreAttributes(target, arguments_str, arguments,
                                           NONE)
      .Check();
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

namespace {

bool IntrinsicHasNoSideEffect(Runtime::FunctionId id) {
// Use macro to include both inlined and non-inlined version of an intrinsic.
#define INTRINSIC_WHITELIST(V)       \
  /* Conversions */                  \
  V(ToInteger)                       \
  V(ToObject)                        \
  V(ToString)                        \
  V(ToLength)                        \
  V(ToNumber)                        \
  /* Type checks */                  \
  V(IsJSReceiver)                    \
  V(IsSmi)                           \
  V(IsArray)                         \
  V(IsFunction)                      \
  V(IsDate)                          \
  V(IsJSProxy)                       \
  V(IsJSMap)                         \
  V(IsJSSet)                         \
  V(IsJSMapIterator)                 \
  V(IsJSSetIterator)                 \
  V(IsJSWeakMap)                     \
  V(IsJSWeakSet)                     \
  V(IsRegExp)                        \
  V(IsTypedArray)                    \
  V(ClassOf)                         \
  /* Loads */                        \
  V(LoadLookupSlotForCall)           \
  /* Arrays */                       \
  V(ArraySpeciesConstructor)         \
  V(NormalizeElements)               \
  V(GetArrayKeys)                    \
  V(HasComplexElements)              \
  V(EstimateNumberOfElements)        \
  /* Errors */                       \
  V(ReThrow)                         \
  V(ThrowReferenceError)             \
  V(ThrowSymbolIteratorInvalid)      \
  V(ThrowIteratorResultNotAnObject)  \
  V(NewTypeError)                    \
  /* Strings */                      \
  V(StringCharCodeAt)                \
  V(StringIndexOf)                   \
  V(StringReplaceOneCharWithString)  \
  V(SubString)                       \
  V(RegExpInternalReplace)           \
  /* Literals */                     \
  V(CreateArrayLiteral)              \
  V(CreateObjectLiteral)             \
  V(CreateRegExpLiteral)             \
  /* Collections */                  \
  V(JSCollectionGetTable)            \
  V(FixedArrayGet)                   \
  V(StringGetRawHashField)           \
  V(GenericHash)                     \
  V(MapIteratorInitialize)           \
  V(MapInitialize)                   \
  /* Called from builtins */         \
  V(StringParseFloat)                \
  V(StringParseInt)                  \
  V(StringCharCodeAtRT)              \
  V(StringIndexOfUnchecked)          \
  V(StringEqual)                     \
  V(SymbolDescriptiveString)         \
  V(GenerateRandomNumbers)           \
  V(ExternalStringGetChar)           \
  V(GlobalPrint)                     \
  V(AllocateInNewSpace)              \
  V(AllocateSeqOneByteString)        \
  V(AllocateSeqTwoByteString)        \
  V(ObjectCreate)                    \
  V(ObjectHasOwnProperty)            \
  V(ArrayIndexOf)                    \
  V(ArrayIncludes_Slow)              \
  V(ArrayIsArray)                    \
  V(ThrowTypeError)                  \
  V(ThrowCalledOnNullOrUndefined)    \
  V(ThrowIncompatibleMethodReceiver) \
  V(ThrowInvalidHint)                \
  V(ThrowNotDateError)               \
  /* Misc. */                        \
  V(ForInPrepare)                    \
  V(Call)                            \
  V(MaxSmi)                          \
  V(HasInPrototypeChain)

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
    case Bytecode::kBitwiseAnd:
    case Bytecode::kBitwiseAndSmi:
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
    case Bytecode::kCreateObjectLiteral:
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
    // Misc.
    case Bytecode::kForInPrepare:
    case Bytecode::kForInContinue:
    case Bytecode::kForInNext:
    case Bytecode::kForInStep:
    case Bytecode::kThrow:
    case Bytecode::kReThrow:
    case Bytecode::kIllegal:
    case Bytecode::kCallJSRuntime:
    case Bytecode::kStackCheck:
    case Bytecode::kReturn:
    case Bytecode::kSetPendingMessage:
      return true;
    default:
      if (FLAG_trace_side_effect_free_debug_evaluate) {
        PrintF("[debug-evaluate] bytecode %s may cause side effect.\n",
               Bytecodes::ToString(bytecode));
      }
      return false;
  }
}

bool BuiltinHasNoSideEffect(Builtins::Name id) {
  switch (id) {
    // Whitelist for builtins.
    // Object builtins.
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
    case Builtins::kObjectHasOwnProperty:
    case Builtins::kObjectPrototypePropertyIsEnumerable:
    case Builtins::kObjectProtoToString:
    // Array builtins.
    case Builtins::kArrayCode:
    case Builtins::kArrayIndexOf:
    case Builtins::kArrayPrototypeValues:
    case Builtins::kArrayIncludes:
    case Builtins::kArrayPrototypeEntries:
    case Builtins::kArrayPrototypeKeys:
    case Builtins::kArrayForEach:
    case Builtins::kArrayEvery:
    case Builtins::kArraySome:
    case Builtins::kArrayReduce:
    case Builtins::kArrayReduceRight:
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
    // String builtins. Strings are immutable.
    case Builtins::kStringFromCharCode:
    case Builtins::kStringFromCodePoint:
    case Builtins::kStringConstructor:
    case Builtins::kStringPrototypeCharAt:
    case Builtins::kStringPrototypeCharCodeAt:
    case Builtins::kStringPrototypeConcat:
    case Builtins::kStringPrototypeEndsWith:
    case Builtins::kStringPrototypeIncludes:
    case Builtins::kStringPrototypeIndexOf:
    case Builtins::kStringPrototypeLastIndexOf:
    case Builtins::kStringPrototypeSlice:
    case Builtins::kStringPrototypeStartsWith:
    case Builtins::kStringPrototypeSubstr:
    case Builtins::kStringPrototypeSubstring:
    case Builtins::kStringPrototypeToString:
    case Builtins::kStringPrototypeToLowerCase:
    case Builtins::kStringPrototypeToUpperCase:
    case Builtins::kStringPrototypeTrim:
    case Builtins::kStringPrototypeTrimLeft:
    case Builtins::kStringPrototypeTrimRight:
    case Builtins::kStringPrototypeValueOf:
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
    // Error builtins.
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

static const Address accessors_with_no_side_effect[] = {
    // Whitelist for accessors.
    FUNCTION_ADDR(Accessors::StringLengthGetter),
    FUNCTION_ADDR(Accessors::ArrayLengthGetter),
    FUNCTION_ADDR(Accessors::FunctionLengthGetter),
    FUNCTION_ADDR(Accessors::FunctionNameGetter),
    FUNCTION_ADDR(Accessors::BoundFunctionLengthGetter),
    FUNCTION_ADDR(Accessors::BoundFunctionNameGetter),
};

}  // anonymous namespace

// static
bool DebugEvaluate::FunctionHasNoSideEffect(Handle<SharedFunctionInfo> info) {
  if (FLAG_trace_side_effect_free_debug_evaluate) {
    PrintF("[debug-evaluate] Checking function %s for side effect.\n",
           info->DebugName()->ToCString().get());
  }

  DCHECK(info->is_compiled());

  if (info->HasBytecodeArray()) {
    // Check bytecodes against whitelist.
    Handle<BytecodeArray> bytecode_array(info->bytecode_array());
    if (FLAG_trace_side_effect_free_debug_evaluate) bytecode_array->Print();
    for (interpreter::BytecodeArrayIterator it(bytecode_array); !it.done();
         it.Advance()) {
      interpreter::Bytecode bytecode = it.current_bytecode();

      if (interpreter::Bytecodes::IsCallRuntime(bytecode)) {
        Runtime::FunctionId id =
            (bytecode == interpreter::Bytecode::kInvokeIntrinsic)
                ? it.GetIntrinsicIdOperand(0)
                : it.GetRuntimeIdOperand(0);
        if (IntrinsicHasNoSideEffect(id)) continue;
        return false;
      }

      if (BytecodeHasNoSideEffect(bytecode)) continue;

      // Did not match whitelist.
      return false;
    }
    return true;
  } else {
    // Check built-ins against whitelist.
    int builtin_index = info->code()->builtin_index();
    if (builtin_index >= 0 && builtin_index < Builtins::builtin_count &&
        BuiltinHasNoSideEffect(static_cast<Builtins::Name>(builtin_index))) {
#ifdef DEBUG
      // TODO(yangguo): Check builtin-to-builtin calls too.
      int mode = RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE);
      bool failed = false;
      for (RelocIterator it(info->code(), mode); !it.done(); it.next()) {
        RelocInfo* rinfo = it.rinfo();
        Address address = rinfo->target_external_reference();
        const Runtime::Function* function = Runtime::FunctionForEntry(address);
        if (function == nullptr) continue;
        if (!IntrinsicHasNoSideEffect(function->function_id)) {
          PrintF("Whitelisted builtin %s calls non-whitelisted intrinsic %s\n",
                 Builtins::name(builtin_index), function->name);
          failed = true;
        }
        CHECK(!failed);
      }
#endif  // DEBUG
      return true;
    }
  }

  return false;
}

// static
bool DebugEvaluate::CallbackHasNoSideEffect(Address function_addr) {
  for (size_t i = 0; i < arraysize(accessors_with_no_side_effect); i++) {
    if (function_addr == accessors_with_no_side_effect[i]) return true;
  }

  if (FLAG_trace_side_effect_free_debug_evaluate) {
    PrintF("[debug-evaluate] API Callback at %p may cause side effect.\n",
           reinterpret_cast<void*>(function_addr));
  }
  return false;
}

}  // namespace internal
}  // namespace v8
