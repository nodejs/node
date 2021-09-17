// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DEFINITIONS_H_
#define V8_BUILTINS_BUILTINS_DEFINITIONS_H_

#include "builtins-generated/bytecodes-builtins-list.h"

// include generated header
#include "torque-generated/builtin-definitions.h"

namespace v8 {
namespace internal {

// CPP: Builtin in C++. Entered via BUILTIN_EXIT frame.
//      Args: name
// TFJ: Builtin in Turbofan, with JS linkage (callable as Javascript function).
//      Args: name, arguments count, explicit argument names...
// TFS: Builtin in Turbofan, with CodeStub linkage.
//      Args: name, explicit argument names...
// TFC: Builtin in Turbofan, with CodeStub linkage and custom descriptor.
//      Args: name, interface descriptor
// TFH: Handlers in Turbofan, with CodeStub linkage.
//      Args: name, interface descriptor
// BCH: Bytecode Handlers, with bytecode dispatch linkage.
//      Args: name, OperandScale, Bytecode
// ASM: Builtin in platform-dependent assembly.
//      Args: name, interface descriptor

// TODO(jgruber): Remove DummyDescriptor once all ASM builtins have been
// properly associated with their descriptor.

#define BUILTIN_LIST_BASE(CPP, TFJ, TFC, TFS, TFH, ASM)                        \
  /* GC write barrirer */                                                      \
  TFC(RecordWriteEmitRememberedSetSaveFP, WriteBarrier)                        \
  TFC(RecordWriteOmitRememberedSetSaveFP, WriteBarrier)                        \
  TFC(RecordWriteEmitRememberedSetIgnoreFP, WriteBarrier)                      \
  TFC(RecordWriteOmitRememberedSetIgnoreFP, WriteBarrier)                      \
  TFC(EphemeronKeyBarrierSaveFP, WriteBarrier)                                 \
  TFC(EphemeronKeyBarrierIgnoreFP, WriteBarrier)                               \
                                                                               \
  /* TSAN support for stores in generated code.*/                              \
  IF_TSAN(TFC, TSANRelaxedStore8IgnoreFP, TSANRelaxedStore)                    \
  IF_TSAN(TFC, TSANRelaxedStore8SaveFP, TSANRelaxedStore)                      \
  IF_TSAN(TFC, TSANRelaxedStore16IgnoreFP, TSANRelaxedStore)                   \
  IF_TSAN(TFC, TSANRelaxedStore16SaveFP, TSANRelaxedStore)                     \
  IF_TSAN(TFC, TSANRelaxedStore32IgnoreFP, TSANRelaxedStore)                   \
  IF_TSAN(TFC, TSANRelaxedStore32SaveFP, TSANRelaxedStore)                     \
  IF_TSAN(TFC, TSANRelaxedStore64IgnoreFP, TSANRelaxedStore)                   \
  IF_TSAN(TFC, TSANRelaxedStore64SaveFP, TSANRelaxedStore)                     \
                                                                               \
  /* TSAN support for loads in generated code.*/                               \
  IF_TSAN(TFC, TSANRelaxedLoad32IgnoreFP, TSANRelaxedLoad)                     \
  IF_TSAN(TFC, TSANRelaxedLoad32SaveFP, TSANRelaxedLoad)                       \
  IF_TSAN(TFC, TSANRelaxedLoad64IgnoreFP, TSANRelaxedLoad)                     \
  IF_TSAN(TFC, TSANRelaxedLoad64SaveFP, TSANRelaxedLoad)                       \
                                                                               \
  /* Adaptor for CPP builtin */                                                \
  TFC(AdaptorWithBuiltinExitFrame, CppBuiltinAdaptor)                          \
                                                                               \
  /* Calls */                                                                  \
  /* ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList) */              \
  ASM(CallFunction_ReceiverIsNullOrUndefined, CallTrampoline)                  \
  ASM(CallFunction_ReceiverIsNotNullOrUndefined, CallTrampoline)               \
  ASM(CallFunction_ReceiverIsAny, CallTrampoline)                              \
  /* ES6 section 9.4.1.1 [[Call]] ( thisArgument, argumentsList) */            \
  ASM(CallBoundFunction, CallTrampoline)                                       \
  /* ES6 section 7.3.12 Call(F, V, [argumentsList]) */                         \
  ASM(Call_ReceiverIsNullOrUndefined, CallTrampoline)                          \
  ASM(Call_ReceiverIsNotNullOrUndefined, CallTrampoline)                       \
  ASM(Call_ReceiverIsAny, CallTrampoline)                                      \
  TFC(Call_ReceiverIsNullOrUndefined_Baseline_Compact,                         \
      CallTrampoline_Baseline_Compact)                                         \
  TFC(Call_ReceiverIsNullOrUndefined_Baseline, CallTrampoline_Baseline)        \
  TFC(Call_ReceiverIsNotNullOrUndefined_Baseline_Compact,                      \
      CallTrampoline_Baseline_Compact)                                         \
  TFC(Call_ReceiverIsNotNullOrUndefined_Baseline, CallTrampoline_Baseline)     \
  TFC(Call_ReceiverIsAny_Baseline_Compact, CallTrampoline_Baseline_Compact)    \
  TFC(Call_ReceiverIsAny_Baseline, CallTrampoline_Baseline)                    \
  TFC(Call_ReceiverIsNullOrUndefined_WithFeedback,                             \
      CallTrampoline_WithFeedback)                                             \
  TFC(Call_ReceiverIsNotNullOrUndefined_WithFeedback,                          \
      CallTrampoline_WithFeedback)                                             \
  TFC(Call_ReceiverIsAny_WithFeedback, CallTrampoline_WithFeedback)            \
                                                                               \
  /* ES6 section 9.5.12[[Call]] ( thisArgument, argumentsList ) */             \
  TFC(CallProxy, CallTrampoline)                                               \
  ASM(CallVarargs, CallVarargs)                                                \
  TFC(CallWithSpread, CallWithSpread)                                          \
  TFC(CallWithSpread_Baseline, CallWithSpread_Baseline)                        \
  TFC(CallWithSpread_WithFeedback, CallWithSpread_WithFeedback)                \
  TFC(CallWithArrayLike, CallWithArrayLike)                                    \
  TFC(CallWithArrayLike_WithFeedback, CallWithArrayLike_WithFeedback)          \
  ASM(CallForwardVarargs, CallForwardVarargs)                                  \
  ASM(CallFunctionForwardVarargs, CallForwardVarargs)                          \
  /* Call an API callback via a {FunctionTemplateInfo}, doing appropriate */   \
  /* access and compatible receiver checks. */                                 \
  TFC(CallFunctionTemplate_CheckAccess, CallFunctionTemplate)                  \
  TFC(CallFunctionTemplate_CheckCompatibleReceiver, CallFunctionTemplate)      \
  TFC(CallFunctionTemplate_CheckAccessAndCompatibleReceiver,                   \
      CallFunctionTemplate)                                                    \
                                                                               \
  /* Construct */                                                              \
  /* ES6 section 9.2.2 [[Construct]] ( argumentsList, newTarget) */            \
  ASM(ConstructFunction, JSTrampoline)                                         \
  /* ES6 section 9.4.1.2 [[Construct]] (argumentsList, newTarget) */           \
  ASM(ConstructBoundFunction, JSTrampoline)                                    \
  ASM(ConstructedNonConstructable, JSTrampoline)                               \
  /* ES6 section 7.3.13 Construct (F, [argumentsList], [newTarget]) */         \
  ASM(Construct, JSTrampoline)                                                 \
  ASM(ConstructVarargs, ConstructVarargs)                                      \
  TFC(ConstructWithSpread, ConstructWithSpread)                                \
  TFC(ConstructWithSpread_Baseline, ConstructWithSpread_Baseline)              \
  TFC(ConstructWithSpread_WithFeedback, ConstructWithSpread_WithFeedback)      \
  TFC(ConstructWithArrayLike, ConstructWithArrayLike)                          \
  TFC(ConstructWithArrayLike_WithFeedback,                                     \
      ConstructWithArrayLike_WithFeedback)                                     \
  ASM(ConstructForwardVarargs, ConstructForwardVarargs)                        \
  ASM(ConstructFunctionForwardVarargs, ConstructForwardVarargs)                \
  TFC(Construct_Baseline, Construct_Baseline)                                  \
  TFC(Construct_WithFeedback, Construct_WithFeedback)                          \
  ASM(JSConstructStubGeneric, Dummy)                                           \
  ASM(JSBuiltinsConstructStub, Dummy)                                          \
  TFC(FastNewObject, FastNewObject)                                            \
  TFS(FastNewClosure, kSharedFunctionInfo, kFeedbackCell)                      \
  /* ES6 section 9.5.14 [[Construct]] ( argumentsList, newTarget) */           \
  TFC(ConstructProxy, JSTrampoline)                                            \
                                                                               \
  /* Apply and entries */                                                      \
  ASM(JSEntry, Dummy)                                                          \
  ASM(JSConstructEntry, Dummy)                                                 \
  ASM(JSRunMicrotasksEntry, RunMicrotasksEntry)                                \
  /* Call a JSValue. */                                                        \
  ASM(JSEntryTrampoline, JSTrampoline)                                         \
  /* Construct a JSValue. */                                                   \
  ASM(JSConstructEntryTrampoline, JSTrampoline)                                \
  ASM(ResumeGeneratorTrampoline, ResumeGenerator)                              \
                                                                               \
  /* String helpers */                                                         \
  TFC(StringCodePointAt, StringAt)                                             \
  TFC(StringFromCodePointAt, StringAtAsString)                                 \
  TFC(StringEqual, Compare)                                                    \
  TFC(StringGreaterThan, Compare)                                              \
  TFC(StringGreaterThanOrEqual, Compare)                                       \
  TFC(StringLessThan, Compare)                                                 \
  TFC(StringLessThanOrEqual, Compare)                                          \
  TFC(StringSubstring, StringSubstring)                                        \
                                                                               \
  /* OrderedHashTable helpers */                                               \
  TFS(OrderedHashTableHealIndex, kTable, kIndex)                               \
                                                                               \
  /* Interpreter */                                                            \
  /* InterpreterEntryTrampoline dispatches to the interpreter to run a */      \
  /* JSFunction in the form of bytecodes */                                    \
  ASM(InterpreterEntryTrampoline, JSTrampoline)                                \
  ASM(InterpreterPushArgsThenCall, InterpreterPushArgsThenCall)                \
  ASM(InterpreterPushUndefinedAndArgsThenCall, InterpreterPushArgsThenCall)    \
  ASM(InterpreterPushArgsThenCallWithFinalSpread, InterpreterPushArgsThenCall) \
  ASM(InterpreterPushArgsThenConstruct, InterpreterPushArgsThenConstruct)      \
  ASM(InterpreterPushArgsThenConstructArrayFunction,                           \
      InterpreterPushArgsThenConstruct)                                        \
  ASM(InterpreterPushArgsThenConstructWithFinalSpread,                         \
      InterpreterPushArgsThenConstruct)                                        \
  ASM(InterpreterEnterAtBytecode, Dummy)                                       \
  ASM(InterpreterEnterAtNextBytecode, Dummy)                                   \
  ASM(InterpreterOnStackReplacement, ContextOnly)                              \
                                                                               \
  /* Baseline Compiler */                                                      \
  ASM(BaselineOutOfLinePrologue, BaselineOutOfLinePrologue)                    \
  ASM(BaselineOnStackReplacement, Void)                                        \
  ASM(BaselineLeaveFrame, BaselineLeaveFrame)                                  \
  ASM(BaselineOrInterpreterEnterAtBytecode, Void)                              \
  ASM(BaselineOrInterpreterEnterAtNextBytecode, Void)                          \
  ASM(InterpreterOnStackReplacement_ToBaseline, Void)                          \
                                                                               \
  /* Code life-cycle */                                                        \
  TFC(CompileLazy, JSTrampoline)                                               \
  TFC(CompileLazyDeoptimizedCode, JSTrampoline)                                \
  TFC(InstantiateAsmJs, JSTrampoline)                                          \
  ASM(NotifyDeoptimized, Dummy)                                                \
  ASM(DeoptimizationEntry_Eager, DeoptimizationEntry)                          \
  ASM(DeoptimizationEntry_Soft, DeoptimizationEntry)                           \
  ASM(DeoptimizationEntry_Bailout, DeoptimizationEntry)                        \
  ASM(DeoptimizationEntry_Lazy, DeoptimizationEntry)                           \
                                                                               \
  /* Trampolines called when returning from a deoptimization that expects   */ \
  /* to continue in a JavaScript builtin to finish the functionality of a   */ \
  /* an TF-inlined version of builtin that has side-effects.                */ \
  /*                                                                        */ \
  /* The trampolines work as follows:                                       */ \
  /*   1. Trampoline restores input register values that                    */ \
  /*      the builtin expects from a BuiltinContinuationFrame.              */ \
  /*   2. Trampoline tears down BuiltinContinuationFrame.                   */ \
  /*   3. Trampoline jumps to the builtin's address.                        */ \
  /*   4. Builtin executes as if invoked by the frame above it.             */ \
  /*   5. When the builtin returns, execution resumes normally in the       */ \
  /*      calling frame, processing any return result from the JavaScript   */ \
  /*      builtin as if it had called the builtin directly.                 */ \
  /*                                                                        */ \
  /* There are two variants of the stub that differ in their handling of a  */ \
  /* value returned by the next frame deeper on the stack. For LAZY deopts, */ \
  /* the return value (e.g. rax on x64) is explicitly passed as an extra    */ \
  /* stack parameter to the JavaScript builtin by the "WithResult"          */ \
  /* trampoline variant. The plain variant is used in EAGER deopt contexts  */ \
  /* and has no such special handling. */                                      \
  ASM(ContinueToCodeStubBuiltin, Dummy)                                        \
  ASM(ContinueToCodeStubBuiltinWithResult, Dummy)                              \
  ASM(ContinueToJavaScriptBuiltin, Dummy)                                      \
  ASM(ContinueToJavaScriptBuiltinWithResult, Dummy)                            \
                                                                               \
  /* API callback handling */                                                  \
  ASM(CallApiCallback, ApiCallback)                                            \
  ASM(CallApiGetter, ApiGetter)                                                \
  CPP(HandleApiCall)                                                           \
  CPP(HandleApiCallAsFunction)                                                 \
  CPP(HandleApiCallAsConstructor)                                              \
                                                                               \
  /* Adapters for Turbofan into runtime */                                     \
  TFC(AllocateInYoungGeneration, Allocate)                                     \
  TFC(AllocateRegularInYoungGeneration, Allocate)                              \
  TFC(AllocateInOldGeneration, Allocate)                                       \
  TFC(AllocateRegularInOldGeneration, Allocate)                                \
                                                                               \
  /* TurboFan support builtins */                                              \
  TFS(CopyFastSmiOrObjectElements, kObject)                                    \
  TFC(GrowFastDoubleElements, GrowArrayElements)                               \
  TFC(GrowFastSmiOrObjectElements, GrowArrayElements)                          \
                                                                               \
  /* Debugger */                                                               \
  TFJ(DebugBreakTrampoline, kDontAdaptArgumentsSentinel)                       \
                                                                               \
  /* Type conversions */                                                       \
  TFC(ToNumber, TypeConversion)                                                \
  TFC(ToNumber_Baseline, TypeConversion_Baseline)                              \
  TFC(ToNumeric_Baseline, TypeConversion_Baseline)                             \
  TFC(PlainPrimitiveToNumber, TypeConversionNoContext)                         \
  TFC(ToNumberConvertBigInt, TypeConversion)                                   \
  TFC(Typeof, Typeof)                                                          \
  TFC(BigIntToI64, BigIntToI64)                                                \
  TFC(BigIntToI32Pair, BigIntToI32Pair)                                        \
  TFC(I64ToBigInt, I64ToBigInt)                                                \
  TFC(I32PairToBigInt, I32PairToBigInt)                                        \
                                                                               \
  /* Type conversions continuations */                                         \
  TFC(ToBooleanLazyDeoptContinuation, SingleParameterOnStack)                  \
                                                                               \
  /* Handlers */                                                               \
  TFH(KeyedLoadIC_PolymorphicName, LoadWithVector)                             \
  TFH(KeyedStoreIC_Megamorphic, Store)                                         \
  TFH(LoadGlobalIC_NoFeedback, LoadGlobalNoFeedback)                           \
  TFH(LoadIC_FunctionPrototype, LoadWithVector)                                \
  TFH(LoadIC_StringLength, LoadWithVector)                                     \
  TFH(LoadIC_StringWrapperLength, LoadWithVector)                              \
  TFH(LoadIC_NoFeedback, LoadNoFeedback)                                       \
  TFH(StoreGlobalIC_Slow, StoreWithVector)                                     \
  TFH(StoreIC_NoFeedback, Store)                                               \
  TFH(KeyedLoadIC_SloppyArguments, LoadWithVector)                             \
  TFH(LoadIndexedInterceptorIC, LoadWithVector)                                \
  TFH(KeyedStoreIC_SloppyArguments_Standard, StoreWithVector)                  \
  TFH(KeyedStoreIC_SloppyArguments_GrowNoTransitionHandleCOW, StoreWithVector) \
  TFH(KeyedStoreIC_SloppyArguments_NoTransitionIgnoreOOB, StoreWithVector)     \
  TFH(KeyedStoreIC_SloppyArguments_NoTransitionHandleCOW, StoreWithVector)     \
  TFH(StoreFastElementIC_Standard, StoreWithVector)                            \
  TFH(StoreFastElementIC_GrowNoTransitionHandleCOW, StoreWithVector)           \
  TFH(StoreFastElementIC_NoTransitionIgnoreOOB, StoreWithVector)               \
  TFH(StoreFastElementIC_NoTransitionHandleCOW, StoreWithVector)               \
  TFH(ElementsTransitionAndStore_Standard, StoreTransition)                    \
  TFH(ElementsTransitionAndStore_GrowNoTransitionHandleCOW, StoreTransition)   \
  TFH(ElementsTransitionAndStore_NoTransitionIgnoreOOB, StoreTransition)       \
  TFH(ElementsTransitionAndStore_NoTransitionHandleCOW, StoreTransition)       \
  TFH(KeyedHasIC_PolymorphicName, LoadWithVector)                              \
  TFH(KeyedHasIC_SloppyArguments, LoadWithVector)                              \
  TFH(HasIndexedInterceptorIC, LoadWithVector)                                 \
                                                                               \
  /* Dynamic check maps */                                                     \
  ASM(DynamicCheckMapsTrampoline, DynamicCheckMaps)                            \
  TFC(DynamicCheckMaps, DynamicCheckMaps)                                      \
  ASM(DynamicCheckMapsWithFeedbackVectorTrampoline,                            \
      DynamicCheckMapsWithFeedbackVector)                                      \
  TFC(DynamicCheckMapsWithFeedbackVector, DynamicCheckMapsWithFeedbackVector)  \
                                                                               \
  /* Microtask helpers */                                                      \
  TFS(EnqueueMicrotask, kMicrotask)                                            \
  ASM(RunMicrotasksTrampoline, RunMicrotasksEntry)                             \
  TFC(RunMicrotasks, RunMicrotasks)                                            \
                                                                               \
  /* Object property helpers */                                                \
  TFS(HasProperty, kObject, kKey)                                              \
  TFS(DeleteProperty, kObject, kKey, kLanguageMode)                            \
  /* ES #sec-copydataproperties */                                             \
  TFS(CopyDataProperties, kTarget, kSource)                                    \
  TFS(SetDataProperties, kTarget, kSource)                                     \
                                                                               \
  /* Abort */                                                                  \
  TFC(Abort, Abort)                                                            \
  TFC(AbortCSAAssert, Abort)                                                   \
                                                                               \
  /* Built-in functions for Javascript */                                      \
  /* Special internal builtins */                                              \
  CPP(EmptyFunction)                                                           \
  CPP(Illegal)                                                                 \
  CPP(StrictPoisonPillThrower)                                                 \
  CPP(UnsupportedThrower)                                                      \
  TFJ(ReturnReceiver, 0, kReceiver)                                            \
                                                                               \
  /* Array */                                                                  \
  TFC(ArrayConstructor, JSTrampoline)                                          \
  TFC(ArrayConstructorImpl, ArrayConstructor)                                  \
  TFC(ArrayNoArgumentConstructor_PackedSmi_DontOverride,                       \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_HoleySmi_DontOverride,                        \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_PackedSmi_DisableAllocationSites,             \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_HoleySmi_DisableAllocationSites,              \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_Packed_DisableAllocationSites,                \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_Holey_DisableAllocationSites,                 \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_PackedDouble_DisableAllocationSites,          \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArrayNoArgumentConstructor_HoleyDouble_DisableAllocationSites,           \
      ArrayNoArgumentConstructor)                                              \
  TFC(ArraySingleArgumentConstructor_PackedSmi_DontOverride,                   \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_HoleySmi_DontOverride,                    \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_PackedSmi_DisableAllocationSites,         \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_HoleySmi_DisableAllocationSites,          \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_Packed_DisableAllocationSites,            \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_Holey_DisableAllocationSites,             \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_PackedDouble_DisableAllocationSites,      \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArraySingleArgumentConstructor_HoleyDouble_DisableAllocationSites,       \
      ArraySingleArgumentConstructor)                                          \
  TFC(ArrayNArgumentsConstructor, ArrayNArgumentsConstructor)                  \
  CPP(ArrayConcat)                                                             \
  /* ES6 #sec-array.prototype.fill */                                          \
  CPP(ArrayPrototypeFill)                                                      \
  /* ES7 #sec-array.prototype.includes */                                      \
  TFS(ArrayIncludesSmiOrObject, kElements, kSearchElement, kLength,            \
      kFromIndex)                                                              \
  TFS(ArrayIncludesPackedDoubles, kElements, kSearchElement, kLength,          \
      kFromIndex)                                                              \
  TFS(ArrayIncludesHoleyDoubles, kElements, kSearchElement, kLength,           \
      kFromIndex)                                                              \
  TFJ(ArrayIncludes, kDontAdaptArgumentsSentinel)                              \
  /* ES6 #sec-array.prototype.indexof */                                       \
  TFS(ArrayIndexOfSmiOrObject, kElements, kSearchElement, kLength, kFromIndex) \
  TFS(ArrayIndexOfPackedDoubles, kElements, kSearchElement, kLength,           \
      kFromIndex)                                                              \
  TFS(ArrayIndexOfHoleyDoubles, kElements, kSearchElement, kLength,            \
      kFromIndex)                                                              \
  TFJ(ArrayIndexOf, kDontAdaptArgumentsSentinel)                               \
  /* ES6 #sec-array.prototype.pop */                                           \
  CPP(ArrayPop)                                                                \
  TFJ(ArrayPrototypePop, kDontAdaptArgumentsSentinel)                          \
  /* ES6 #sec-array.prototype.push */                                          \
  CPP(ArrayPush)                                                               \
  TFJ(ArrayPrototypePush, kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-array.prototype.shift */                                         \
  CPP(ArrayShift)                                                              \
  /* ES6 #sec-array.prototype.unshift */                                       \
  CPP(ArrayUnshift)                                                            \
  /* Support for Array.from and other array-copying idioms */                  \
  TFS(CloneFastJSArray, kSource)                                               \
  TFS(CloneFastJSArrayFillingHoles, kSource)                                   \
  TFS(ExtractFastJSArray, kSource, kBegin, kCount)                             \
  /* ES6 #sec-array.prototype.entries */                                       \
  TFJ(ArrayPrototypeEntries, 0, kReceiver)                                     \
  /* ES6 #sec-array.prototype.keys */                                          \
  TFJ(ArrayPrototypeKeys, 0, kReceiver)                                        \
  /* ES6 #sec-array.prototype.values */                                        \
  TFJ(ArrayPrototypeValues, 0, kReceiver)                                      \
  /* ES6 #sec-%arrayiteratorprototype%.next */                                 \
  TFJ(ArrayIteratorPrototypeNext, 0, kReceiver)                                \
  /* https://tc39.github.io/proposal-flatMap/#sec-FlattenIntoArray */          \
  TFS(FlattenIntoArray, kTarget, kSource, kSourceLength, kStart, kDepth)       \
  TFS(FlatMapIntoArray, kTarget, kSource, kSourceLength, kStart, kDepth,       \
      kMapperFunction, kThisArg)                                               \
  /* https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flat */      \
  TFJ(ArrayPrototypeFlat, kDontAdaptArgumentsSentinel)                         \
  /* https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatMap */   \
  TFJ(ArrayPrototypeFlatMap, kDontAdaptArgumentsSentinel)                      \
                                                                               \
  /* ArrayBuffer */                                                            \
  /* ES #sec-arraybuffer-constructor */                                        \
  CPP(ArrayBufferConstructor)                                                  \
  CPP(ArrayBufferConstructor_DoNotInitialize)                                  \
  CPP(ArrayBufferPrototypeSlice)                                               \
  /* https://tc39.es/proposal-resizablearraybuffer/ */                         \
  CPP(ArrayBufferPrototypeResize)                                              \
                                                                               \
  /* AsyncFunction */                                                          \
  TFS(AsyncFunctionEnter, kClosure, kReceiver)                                 \
  TFS(AsyncFunctionReject, kAsyncFunctionObject, kReason, kCanSuspend)         \
  TFS(AsyncFunctionResolve, kAsyncFunctionObject, kValue, kCanSuspend)         \
  TFC(AsyncFunctionLazyDeoptContinuation, AsyncFunctionStackParameter)         \
  TFS(AsyncFunctionAwaitCaught, kAsyncFunctionObject, kValue)                  \
  TFS(AsyncFunctionAwaitUncaught, kAsyncFunctionObject, kValue)                \
  TFJ(AsyncFunctionAwaitRejectClosure, 1, kReceiver, kSentError)               \
  TFJ(AsyncFunctionAwaitResolveClosure, 1, kReceiver, kSentValue)              \
                                                                               \
  /* BigInt */                                                                 \
  CPP(BigIntConstructor)                                                       \
  CPP(BigIntAsUintN)                                                           \
  CPP(BigIntAsIntN)                                                            \
  CPP(BigIntPrototypeToLocaleString)                                           \
  CPP(BigIntPrototypeToString)                                                 \
  CPP(BigIntPrototypeValueOf)                                                  \
                                                                               \
  /* CallSite */                                                               \
  CPP(CallSitePrototypeGetColumnNumber)                                        \
  CPP(CallSitePrototypeGetEnclosingColumnNumber)                               \
  CPP(CallSitePrototypeGetEnclosingLineNumber)                                 \
  CPP(CallSitePrototypeGetEvalOrigin)                                          \
  CPP(CallSitePrototypeGetFileName)                                            \
  CPP(CallSitePrototypeGetFunction)                                            \
  CPP(CallSitePrototypeGetFunctionName)                                        \
  CPP(CallSitePrototypeGetLineNumber)                                          \
  CPP(CallSitePrototypeGetMethodName)                                          \
  CPP(CallSitePrototypeGetPosition)                                            \
  CPP(CallSitePrototypeGetPromiseIndex)                                        \
  CPP(CallSitePrototypeGetScriptNameOrSourceURL)                               \
  CPP(CallSitePrototypeGetThis)                                                \
  CPP(CallSitePrototypeGetTypeName)                                            \
  CPP(CallSitePrototypeIsAsync)                                                \
  CPP(CallSitePrototypeIsConstructor)                                          \
  CPP(CallSitePrototypeIsEval)                                                 \
  CPP(CallSitePrototypeIsNative)                                               \
  CPP(CallSitePrototypeIsPromiseAll)                                           \
  CPP(CallSitePrototypeIsToplevel)                                             \
  CPP(CallSitePrototypeToString)                                               \
                                                                               \
  /* Console */                                                                \
  CPP(ConsoleDebug)                                                            \
  CPP(ConsoleError)                                                            \
  CPP(ConsoleInfo)                                                             \
  CPP(ConsoleLog)                                                              \
  CPP(ConsoleWarn)                                                             \
  CPP(ConsoleDir)                                                              \
  CPP(ConsoleDirXml)                                                           \
  CPP(ConsoleTable)                                                            \
  CPP(ConsoleTrace)                                                            \
  CPP(ConsoleGroup)                                                            \
  CPP(ConsoleGroupCollapsed)                                                   \
  CPP(ConsoleGroupEnd)                                                         \
  CPP(ConsoleClear)                                                            \
  CPP(ConsoleCount)                                                            \
  CPP(ConsoleCountReset)                                                       \
  CPP(ConsoleAssert)                                                           \
  CPP(ConsoleProfile)                                                          \
  CPP(ConsoleProfileEnd)                                                       \
  CPP(ConsoleTime)                                                             \
  CPP(ConsoleTimeLog)                                                          \
  CPP(ConsoleTimeEnd)                                                          \
  CPP(ConsoleTimeStamp)                                                        \
  CPP(ConsoleContext)                                                          \
                                                                               \
  /* DataView */                                                               \
  /* ES #sec-dataview-constructor */                                           \
  CPP(DataViewConstructor)                                                     \
                                                                               \
  /* Date */                                                                   \
  /* ES #sec-date-constructor */                                               \
  CPP(DateConstructor)                                                         \
  /* ES6 #sec-date.prototype.getdate */                                        \
  TFJ(DatePrototypeGetDate, 0, kReceiver)                                      \
  /* ES6 #sec-date.prototype.getday */                                         \
  TFJ(DatePrototypeGetDay, 0, kReceiver)                                       \
  /* ES6 #sec-date.prototype.getfullyear */                                    \
  TFJ(DatePrototypeGetFullYear, 0, kReceiver)                                  \
  /* ES6 #sec-date.prototype.gethours */                                       \
  TFJ(DatePrototypeGetHours, 0, kReceiver)                                     \
  /* ES6 #sec-date.prototype.getmilliseconds */                                \
  TFJ(DatePrototypeGetMilliseconds, 0, kReceiver)                              \
  /* ES6 #sec-date.prototype.getminutes */                                     \
  TFJ(DatePrototypeGetMinutes, 0, kReceiver)                                   \
  /* ES6 #sec-date.prototype.getmonth */                                       \
  TFJ(DatePrototypeGetMonth, 0, kReceiver)                                     \
  /* ES6 #sec-date.prototype.getseconds */                                     \
  TFJ(DatePrototypeGetSeconds, 0, kReceiver)                                   \
  /* ES6 #sec-date.prototype.gettime */                                        \
  TFJ(DatePrototypeGetTime, 0, kReceiver)                                      \
  /* ES6 #sec-date.prototype.gettimezoneoffset */                              \
  TFJ(DatePrototypeGetTimezoneOffset, 0, kReceiver)                            \
  /* ES6 #sec-date.prototype.getutcdate */                                     \
  TFJ(DatePrototypeGetUTCDate, 0, kReceiver)                                   \
  /* ES6 #sec-date.prototype.getutcday */                                      \
  TFJ(DatePrototypeGetUTCDay, 0, kReceiver)                                    \
  /* ES6 #sec-date.prototype.getutcfullyear */                                 \
  TFJ(DatePrototypeGetUTCFullYear, 0, kReceiver)                               \
  /* ES6 #sec-date.prototype.getutchours */                                    \
  TFJ(DatePrototypeGetUTCHours, 0, kReceiver)                                  \
  /* ES6 #sec-date.prototype.getutcmilliseconds */                             \
  TFJ(DatePrototypeGetUTCMilliseconds, 0, kReceiver)                           \
  /* ES6 #sec-date.prototype.getutcminutes */                                  \
  TFJ(DatePrototypeGetUTCMinutes, 0, kReceiver)                                \
  /* ES6 #sec-date.prototype.getutcmonth */                                    \
  TFJ(DatePrototypeGetUTCMonth, 0, kReceiver)                                  \
  /* ES6 #sec-date.prototype.getutcseconds */                                  \
  TFJ(DatePrototypeGetUTCSeconds, 0, kReceiver)                                \
  /* ES6 #sec-date.prototype.valueof */                                        \
  TFJ(DatePrototypeValueOf, 0, kReceiver)                                      \
  /* ES6 #sec-date.prototype-@@toprimitive */                                  \
  TFJ(DatePrototypeToPrimitive, 1, kReceiver, kHint)                           \
  CPP(DatePrototypeGetYear)                                                    \
  CPP(DatePrototypeSetYear)                                                    \
  CPP(DateNow)                                                                 \
  CPP(DateParse)                                                               \
  CPP(DatePrototypeSetDate)                                                    \
  CPP(DatePrototypeSetFullYear)                                                \
  CPP(DatePrototypeSetHours)                                                   \
  CPP(DatePrototypeSetMilliseconds)                                            \
  CPP(DatePrototypeSetMinutes)                                                 \
  CPP(DatePrototypeSetMonth)                                                   \
  CPP(DatePrototypeSetSeconds)                                                 \
  CPP(DatePrototypeSetTime)                                                    \
  CPP(DatePrototypeSetUTCDate)                                                 \
  CPP(DatePrototypeSetUTCFullYear)                                             \
  CPP(DatePrototypeSetUTCHours)                                                \
  CPP(DatePrototypeSetUTCMilliseconds)                                         \
  CPP(DatePrototypeSetUTCMinutes)                                              \
  CPP(DatePrototypeSetUTCMonth)                                                \
  CPP(DatePrototypeSetUTCSeconds)                                              \
  CPP(DatePrototypeToDateString)                                               \
  CPP(DatePrototypeToISOString)                                                \
  CPP(DatePrototypeToUTCString)                                                \
  CPP(DatePrototypeToString)                                                   \
  CPP(DatePrototypeToTimeString)                                               \
  CPP(DatePrototypeToJson)                                                     \
  CPP(DateUTC)                                                                 \
                                                                               \
  /* Error */                                                                  \
  CPP(ErrorConstructor)                                                        \
  CPP(ErrorCaptureStackTrace)                                                  \
  CPP(ErrorPrototypeToString)                                                  \
                                                                               \
  /* Function */                                                               \
  CPP(FunctionConstructor)                                                     \
  ASM(FunctionPrototypeApply, JSTrampoline)                                    \
  CPP(FunctionPrototypeBind)                                                   \
  ASM(FunctionPrototypeCall, JSTrampoline)                                     \
  /* ES6 #sec-function.prototype.tostring */                                   \
  CPP(FunctionPrototypeToString)                                               \
                                                                               \
  /* Belongs to Objects but is a dependency of GeneratorPrototypeResume */     \
  TFS(CreateIterResultObject, kValue, kDone)                                   \
                                                                               \
  /* Generator and Async */                                                    \
  TFS(CreateGeneratorObject, kClosure, kReceiver)                              \
  CPP(GeneratorFunctionConstructor)                                            \
  /* ES6 #sec-generator.prototype.next */                                      \
  TFJ(GeneratorPrototypeNext, kDontAdaptArgumentsSentinel)                     \
  /* ES6 #sec-generator.prototype.return */                                    \
  TFJ(GeneratorPrototypeReturn, kDontAdaptArgumentsSentinel)                   \
  /* ES6 #sec-generator.prototype.throw */                                     \
  TFJ(GeneratorPrototypeThrow, kDontAdaptArgumentsSentinel)                    \
  CPP(AsyncFunctionConstructor)                                                \
  TFC(SuspendGeneratorBaseline, SuspendGeneratorBaseline)                      \
  TFC(ResumeGeneratorBaseline, ResumeGeneratorBaseline)                        \
                                                                               \
  /* Iterator Protocol */                                                      \
  TFC(GetIteratorWithFeedbackLazyDeoptContinuation, GetIteratorStackParameter) \
                                                                               \
  /* Global object */                                                          \
  CPP(GlobalDecodeURI)                                                         \
  CPP(GlobalDecodeURIComponent)                                                \
  CPP(GlobalEncodeURI)                                                         \
  CPP(GlobalEncodeURIComponent)                                                \
  CPP(GlobalEscape)                                                            \
  CPP(GlobalUnescape)                                                          \
  CPP(GlobalEval)                                                              \
  /* ES6 #sec-isfinite-number */                                               \
  TFJ(GlobalIsFinite, 1, kReceiver, kNumber)                                   \
  /* ES6 #sec-isnan-number */                                                  \
  TFJ(GlobalIsNaN, 1, kReceiver, kNumber)                                      \
                                                                               \
  /* JSON */                                                                   \
  CPP(JsonParse)                                                               \
  CPP(JsonStringify)                                                           \
                                                                               \
  /* ICs */                                                                    \
  TFH(LoadIC, LoadWithVector)                                                  \
  TFH(LoadIC_Megamorphic, LoadWithVector)                                      \
  TFH(LoadIC_Noninlined, LoadWithVector)                                       \
  TFH(LoadICTrampoline, Load)                                                  \
  TFH(LoadICBaseline, LoadBaseline)                                            \
  TFH(LoadICTrampoline_Megamorphic, Load)                                      \
  TFH(LoadSuperIC, LoadWithReceiverAndVector)                                  \
  TFH(LoadSuperICBaseline, LoadWithReceiverBaseline)                           \
  TFH(KeyedLoadIC, LoadWithVector)                                             \
  TFH(KeyedLoadIC_Megamorphic, LoadWithVector)                                 \
  TFH(KeyedLoadICTrampoline, Load)                                             \
  TFH(KeyedLoadICBaseline, LoadBaseline)                                       \
  TFH(KeyedLoadICTrampoline_Megamorphic, Load)                                 \
  TFH(StoreGlobalIC, StoreGlobalWithVector)                                    \
  TFH(StoreGlobalICTrampoline, StoreGlobal)                                    \
  TFH(StoreGlobalICBaseline, StoreGlobalBaseline)                              \
  TFH(StoreIC, StoreWithVector)                                                \
  TFH(StoreICTrampoline, Store)                                                \
  TFH(StoreICBaseline, StoreBaseline)                                          \
  TFH(KeyedStoreIC, StoreWithVector)                                           \
  TFH(KeyedStoreICTrampoline, Store)                                           \
  TFH(KeyedStoreICBaseline, StoreBaseline)                                     \
  TFH(StoreInArrayLiteralIC, StoreWithVector)                                  \
  TFH(StoreInArrayLiteralICBaseline, StoreBaseline)                            \
  TFH(LookupContextBaseline, LookupBaseline)                                   \
  TFH(LookupContextInsideTypeofBaseline, LookupBaseline)                       \
  TFH(LoadGlobalIC, LoadGlobalWithVector)                                      \
  TFH(LoadGlobalICInsideTypeof, LoadGlobalWithVector)                          \
  TFH(LoadGlobalICTrampoline, LoadGlobal)                                      \
  TFH(LoadGlobalICBaseline, LoadGlobalBaseline)                                \
  TFH(LoadGlobalICInsideTypeofTrampoline, LoadGlobal)                          \
  TFH(LoadGlobalICInsideTypeofBaseline, LoadGlobalBaseline)                    \
  TFH(LookupGlobalICBaseline, LookupBaseline)                                  \
  TFH(LookupGlobalICInsideTypeofBaseline, LookupBaseline)                      \
  TFH(CloneObjectIC, CloneObjectWithVector)                                    \
  TFH(CloneObjectICBaseline, CloneObjectBaseline)                              \
  TFH(CloneObjectIC_Slow, CloneObjectWithVector)                               \
  TFH(KeyedHasIC, LoadWithVector)                                              \
  TFH(KeyedHasICBaseline, LoadBaseline)                                        \
  TFH(KeyedHasIC_Megamorphic, LoadWithVector)                                  \
                                                                               \
  /* IterableToList */                                                         \
  /* ES #sec-iterabletolist */                                                 \
  TFS(IterableToList, kIterable, kIteratorFn)                                  \
  TFS(IterableToFixedArray, kIterable, kIteratorFn)                            \
  TFS(IterableToListWithSymbolLookup, kIterable)                               \
  TFS(IterableToFixedArrayWithSymbolLookupSlow, kIterable)                     \
  TFS(IterableToListMayPreserveHoles, kIterable, kIteratorFn)                  \
  IF_WASM(TFS, IterableToFixedArrayForWasm, kIterable, kExpectedLength)        \
                                                                               \
  /* #sec-createstringlistfromiterable */                                      \
  TFS(StringListFromIterable, kIterable)                                       \
                                                                               \
  /* Map */                                                                    \
  TFS(FindOrderedHashMapEntry, kTable, kKey)                                   \
  TFJ(MapConstructor, kDontAdaptArgumentsSentinel)                             \
  TFJ(MapPrototypeSet, 2, kReceiver, kKey, kValue)                             \
  TFJ(MapPrototypeDelete, 1, kReceiver, kKey)                                  \
  TFJ(MapPrototypeGet, 1, kReceiver, kKey)                                     \
  TFJ(MapPrototypeHas, 1, kReceiver, kKey)                                     \
  CPP(MapPrototypeClear)                                                       \
  /* ES #sec-map.prototype.entries */                                          \
  TFJ(MapPrototypeEntries, 0, kReceiver)                                       \
  /* ES #sec-get-map.prototype.size */                                         \
  TFJ(MapPrototypeGetSize, 0, kReceiver)                                       \
  /* ES #sec-map.prototype.forEach */                                          \
  TFJ(MapPrototypeForEach, kDontAdaptArgumentsSentinel)                        \
  /* ES #sec-map.prototype.keys */                                             \
  TFJ(MapPrototypeKeys, 0, kReceiver)                                          \
  /* ES #sec-map.prototype.values */                                           \
  TFJ(MapPrototypeValues, 0, kReceiver)                                        \
  /* ES #sec-%mapiteratorprototype%.next */                                    \
  TFJ(MapIteratorPrototypeNext, 0, kReceiver)                                  \
  TFS(MapIteratorToList, kSource)                                              \
                                                                               \
  /* ES #sec-number-constructor */                                             \
  CPP(NumberPrototypeToExponential)                                            \
  CPP(NumberPrototypeToFixed)                                                  \
  CPP(NumberPrototypeToLocaleString)                                           \
  CPP(NumberPrototypeToPrecision)                                              \
  TFC(SameValue, Compare)                                                      \
  TFC(SameValueNumbersOnly, Compare)                                           \
                                                                               \
  /* Binary ops with feedback collection */                                    \
  TFC(Add_Baseline, BinaryOp_Baseline)                                         \
  TFC(Subtract_Baseline, BinaryOp_Baseline)                                    \
  TFC(Multiply_Baseline, BinaryOp_Baseline)                                    \
  TFC(Divide_Baseline, BinaryOp_Baseline)                                      \
  TFC(Modulus_Baseline, BinaryOp_Baseline)                                     \
  TFC(Exponentiate_Baseline, BinaryOp_Baseline)                                \
  TFC(BitwiseAnd_Baseline, BinaryOp_Baseline)                                  \
  TFC(BitwiseOr_Baseline, BinaryOp_Baseline)                                   \
  TFC(BitwiseXor_Baseline, BinaryOp_Baseline)                                  \
  TFC(ShiftLeft_Baseline, BinaryOp_Baseline)                                   \
  TFC(ShiftRight_Baseline, BinaryOp_Baseline)                                  \
  TFC(ShiftRightLogical_Baseline, BinaryOp_Baseline)                           \
                                                                               \
  TFC(Add_WithFeedback, BinaryOp_WithFeedback)                                 \
  TFC(Subtract_WithFeedback, BinaryOp_WithFeedback)                            \
  TFC(Multiply_WithFeedback, BinaryOp_WithFeedback)                            \
  TFC(Divide_WithFeedback, BinaryOp_WithFeedback)                              \
  TFC(Modulus_WithFeedback, BinaryOp_WithFeedback)                             \
  TFC(Exponentiate_WithFeedback, BinaryOp_WithFeedback)                        \
  TFC(BitwiseAnd_WithFeedback, BinaryOp_WithFeedback)                          \
  TFC(BitwiseOr_WithFeedback, BinaryOp_WithFeedback)                           \
  TFC(BitwiseXor_WithFeedback, BinaryOp_WithFeedback)                          \
  TFC(ShiftLeft_WithFeedback, BinaryOp_WithFeedback)                           \
  TFC(ShiftRight_WithFeedback, BinaryOp_WithFeedback)                          \
  TFC(ShiftRightLogical_WithFeedback, BinaryOp_WithFeedback)                   \
                                                                               \
  /* Compare ops with feedback collection */                                   \
  TFC(Equal_Baseline, Compare_Baseline)                                        \
  TFC(StrictEqual_Baseline, Compare_Baseline)                                  \
  TFC(LessThan_Baseline, Compare_Baseline)                                     \
  TFC(GreaterThan_Baseline, Compare_Baseline)                                  \
  TFC(LessThanOrEqual_Baseline, Compare_Baseline)                              \
  TFC(GreaterThanOrEqual_Baseline, Compare_Baseline)                           \
                                                                               \
  TFC(Equal_WithFeedback, Compare_WithFeedback)                                \
  TFC(StrictEqual_WithFeedback, Compare_WithFeedback)                          \
  TFC(LessThan_WithFeedback, Compare_WithFeedback)                             \
  TFC(GreaterThan_WithFeedback, Compare_WithFeedback)                          \
  TFC(LessThanOrEqual_WithFeedback, Compare_WithFeedback)                      \
  TFC(GreaterThanOrEqual_WithFeedback, Compare_WithFeedback)                   \
                                                                               \
  /* Unary ops with feedback collection */                                     \
  TFC(BitwiseNot_Baseline, UnaryOp_Baseline)                                   \
  TFC(Decrement_Baseline, UnaryOp_Baseline)                                    \
  TFC(Increment_Baseline, UnaryOp_Baseline)                                    \
  TFC(Negate_Baseline, UnaryOp_Baseline)                                       \
  TFC(BitwiseNot_WithFeedback, UnaryOp_WithFeedback)                           \
  TFC(Decrement_WithFeedback, UnaryOp_WithFeedback)                            \
  TFC(Increment_WithFeedback, UnaryOp_WithFeedback)                            \
  TFC(Negate_WithFeedback, UnaryOp_WithFeedback)                               \
                                                                               \
  /* Object */                                                                 \
  /* ES #sec-object-constructor */                                             \
  TFJ(ObjectAssign, kDontAdaptArgumentsSentinel)                               \
  /* ES #sec-object.create */                                                  \
  TFJ(ObjectCreate, kDontAdaptArgumentsSentinel)                               \
  CPP(ObjectDefineGetter)                                                      \
  CPP(ObjectDefineProperties)                                                  \
  CPP(ObjectDefineProperty)                                                    \
  CPP(ObjectDefineSetter)                                                      \
  TFJ(ObjectEntries, 1, kReceiver, kObject)                                    \
  CPP(ObjectFreeze)                                                            \
  TFJ(ObjectGetOwnPropertyDescriptor, kDontAdaptArgumentsSentinel)             \
  CPP(ObjectGetOwnPropertyDescriptors)                                         \
  TFJ(ObjectGetOwnPropertyNames, 1, kReceiver, kObject)                        \
  CPP(ObjectGetOwnPropertySymbols)                                             \
  TFJ(ObjectHasOwn, 2, kReceiver, kObject, kKey)                               \
  TFJ(ObjectIs, 2, kReceiver, kLeft, kRight)                                   \
  CPP(ObjectIsFrozen)                                                          \
  CPP(ObjectIsSealed)                                                          \
  TFJ(ObjectKeys, 1, kReceiver, kObject)                                       \
  CPP(ObjectLookupGetter)                                                      \
  CPP(ObjectLookupSetter)                                                      \
  /* ES6 #sec-object.prototype.hasownproperty */                               \
  TFJ(ObjectPrototypeHasOwnProperty, 1, kReceiver, kKey)                       \
  TFJ(ObjectPrototypeIsPrototypeOf, 1, kReceiver, kValue)                      \
  CPP(ObjectPrototypePropertyIsEnumerable)                                     \
  CPP(ObjectPrototypeGetProto)                                                 \
  CPP(ObjectPrototypeSetProto)                                                 \
  CPP(ObjectSeal)                                                              \
  TFS(ObjectToString, kReceiver)                                               \
  TFJ(ObjectValues, 1, kReceiver, kObject)                                     \
                                                                               \
  /* instanceof */                                                             \
  TFC(OrdinaryHasInstance, Compare)                                            \
  TFC(InstanceOf, Compare)                                                     \
  TFC(InstanceOf_WithFeedback, Compare_WithFeedback)                           \
  TFC(InstanceOf_Baseline, Compare_Baseline)                                   \
                                                                               \
  /* for-in */                                                                 \
  TFS(ForInEnumerate, kReceiver)                                               \
  TFC(ForInPrepare, ForInPrepare)                                              \
  TFS(ForInFilter, kKey, kObject)                                              \
                                                                               \
  /* Reflect */                                                                \
  ASM(ReflectApply, JSTrampoline)                                              \
  ASM(ReflectConstruct, JSTrampoline)                                          \
  CPP(ReflectDefineProperty)                                                   \
  CPP(ReflectGetOwnPropertyDescriptor)                                         \
  CPP(ReflectOwnKeys)                                                          \
  CPP(ReflectSet)                                                              \
                                                                               \
  /* RegExp */                                                                 \
  CPP(RegExpCapture1Getter)                                                    \
  CPP(RegExpCapture2Getter)                                                    \
  CPP(RegExpCapture3Getter)                                                    \
  CPP(RegExpCapture4Getter)                                                    \
  CPP(RegExpCapture5Getter)                                                    \
  CPP(RegExpCapture6Getter)                                                    \
  CPP(RegExpCapture7Getter)                                                    \
  CPP(RegExpCapture8Getter)                                                    \
  CPP(RegExpCapture9Getter)                                                    \
  /* ES #sec-regexp-pattern-flags */                                           \
  TFJ(RegExpConstructor, 2, kReceiver, kPattern, kFlags)                       \
  CPP(RegExpInputGetter)                                                       \
  CPP(RegExpInputSetter)                                                       \
  CPP(RegExpLastMatchGetter)                                                   \
  CPP(RegExpLastParenGetter)                                                   \
  CPP(RegExpLeftContextGetter)                                                 \
  /* ES #sec-regexp.prototype.compile */                                       \
  TFJ(RegExpPrototypeCompile, 2, kReceiver, kPattern, kFlags)                  \
  CPP(RegExpPrototypeToString)                                                 \
  CPP(RegExpRightContextGetter)                                                \
                                                                               \
  /* RegExp helpers */                                                         \
  TFS(RegExpExecAtom, kRegExp, kString, kLastIndex, kMatchInfo)                \
  TFS(RegExpExecInternal, kRegExp, kString, kLastIndex, kMatchInfo)            \
  ASM(RegExpInterpreterTrampoline, CCall)                                      \
  ASM(RegExpExperimentalTrampoline, CCall)                                     \
                                                                               \
  /* Set */                                                                    \
  TFJ(SetConstructor, kDontAdaptArgumentsSentinel)                             \
  TFJ(SetPrototypeHas, 1, kReceiver, kKey)                                     \
  TFJ(SetPrototypeAdd, 1, kReceiver, kKey)                                     \
  TFJ(SetPrototypeDelete, 1, kReceiver, kKey)                                  \
  CPP(SetPrototypeClear)                                                       \
  /* ES #sec-set.prototype.entries */                                          \
  TFJ(SetPrototypeEntries, 0, kReceiver)                                       \
  /* ES #sec-get-set.prototype.size */                                         \
  TFJ(SetPrototypeGetSize, 0, kReceiver)                                       \
  /* ES #sec-set.prototype.foreach */                                          \
  TFJ(SetPrototypeForEach, kDontAdaptArgumentsSentinel)                        \
  /* ES #sec-set.prototype.values */                                           \
  TFJ(SetPrototypeValues, 0, kReceiver)                                        \
  /* ES #sec-%setiteratorprototype%.next */                                    \
  TFJ(SetIteratorPrototypeNext, 0, kReceiver)                                  \
  TFS(SetOrSetIteratorToList, kSource)                                         \
                                                                               \
  /* SharedArrayBuffer */                                                      \
  CPP(SharedArrayBufferPrototypeGetByteLength)                                 \
  CPP(SharedArrayBufferPrototypeSlice)                                         \
  /* https://tc39.es/proposal-resizablearraybuffer/ */                         \
  CPP(SharedArrayBufferPrototypeGrow)                                          \
                                                                               \
  TFJ(AtomicsLoad, 2, kReceiver, kArray, kIndex)                               \
  TFJ(AtomicsStore, 3, kReceiver, kArray, kIndex, kValue)                      \
  TFJ(AtomicsExchange, 3, kReceiver, kArray, kIndex, kValue)                   \
  TFJ(AtomicsCompareExchange, 4, kReceiver, kArray, kIndex, kOldValue,         \
      kNewValue)                                                               \
  TFJ(AtomicsAdd, 3, kReceiver, kArray, kIndex, kValue)                        \
  TFJ(AtomicsSub, 3, kReceiver, kArray, kIndex, kValue)                        \
  TFJ(AtomicsAnd, 3, kReceiver, kArray, kIndex, kValue)                        \
  TFJ(AtomicsOr, 3, kReceiver, kArray, kIndex, kValue)                         \
  TFJ(AtomicsXor, 3, kReceiver, kArray, kIndex, kValue)                        \
  CPP(AtomicsNotify)                                                           \
  CPP(AtomicsIsLockFree)                                                       \
  CPP(AtomicsWait)                                                             \
  CPP(AtomicsWaitAsync)                                                        \
                                                                               \
  /* String */                                                                 \
  /* ES #sec-string.fromcodepoint */                                           \
  CPP(StringFromCodePoint)                                                     \
  /* ES6 #sec-string.fromcharcode */                                           \
  TFJ(StringFromCharCode, kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-string.prototype.lastindexof */                                  \
  CPP(StringPrototypeLastIndexOf)                                              \
  /* ES #sec-string.prototype.matchAll */                                      \
  TFJ(StringPrototypeMatchAll, 1, kReceiver, kRegexp)                          \
  /* ES6 #sec-string.prototype.localecompare */                                \
  CPP(StringPrototypeLocaleCompare)                                            \
  /* ES6 #sec-string.prototype.replace */                                      \
  TFJ(StringPrototypeReplace, 2, kReceiver, kSearch, kReplace)                 \
  /* ES6 #sec-string.prototype.split */                                        \
  TFJ(StringPrototypeSplit, kDontAdaptArgumentsSentinel)                       \
  /* ES6 #sec-string.raw */                                                    \
  CPP(StringRaw)                                                               \
                                                                               \
  /* Symbol */                                                                 \
  /* ES #sec-symbol-constructor */                                             \
  CPP(SymbolConstructor)                                                       \
  /* ES6 #sec-symbol.for */                                                    \
  CPP(SymbolFor)                                                               \
  /* ES6 #sec-symbol.keyfor */                                                 \
  CPP(SymbolKeyFor)                                                            \
                                                                               \
  /* TypedArray */                                                             \
  /* ES #sec-typedarray-constructors */                                        \
  TFJ(TypedArrayBaseConstructor, 0, kReceiver)                                 \
  TFJ(TypedArrayConstructor, kDontAdaptArgumentsSentinel)                      \
  CPP(TypedArrayPrototypeBuffer)                                               \
  /* ES6 #sec-get-%typedarray%.prototype.bytelength */                         \
  TFJ(TypedArrayPrototypeByteLength, 0, kReceiver)                             \
  /* ES6 #sec-get-%typedarray%.prototype.byteoffset */                         \
  TFJ(TypedArrayPrototypeByteOffset, 0, kReceiver)                             \
  /* ES6 #sec-get-%typedarray%.prototype.length */                             \
  TFJ(TypedArrayPrototypeLength, 0, kReceiver)                                 \
  /* ES6 #sec-%typedarray%.prototype.copywithin */                             \
  CPP(TypedArrayPrototypeCopyWithin)                                           \
  /* ES6 #sec-%typedarray%.prototype.fill */                                   \
  CPP(TypedArrayPrototypeFill)                                                 \
  /* ES7 #sec-%typedarray%.prototype.includes */                               \
  CPP(TypedArrayPrototypeIncludes)                                             \
  /* ES6 #sec-%typedarray%.prototype.indexof */                                \
  CPP(TypedArrayPrototypeIndexOf)                                              \
  /* ES6 #sec-%typedarray%.prototype.lastindexof */                            \
  CPP(TypedArrayPrototypeLastIndexOf)                                          \
  /* ES6 #sec-%typedarray%.prototype.reverse */                                \
  CPP(TypedArrayPrototypeReverse)                                              \
  /* ES6 #sec-get-%typedarray%.prototype-@@tostringtag */                      \
  TFJ(TypedArrayPrototypeToStringTag, 0, kReceiver)                            \
  /* ES6 %TypedArray%.prototype.map */                                         \
  TFJ(TypedArrayPrototypeMap, kDontAdaptArgumentsSentinel)                     \
                                                                               \
  /* Wasm */                                                                   \
  IF_WASM(ASM, GenericJSToWasmWrapper, Dummy)                                  \
  IF_WASM(ASM, WasmCompileLazy, Dummy)                                         \
  IF_WASM(ASM, WasmDebugBreak, Dummy)                                          \
  IF_WASM(ASM, WasmOnStackReplace, Dummy)                                      \
  IF_WASM(TFC, WasmFloat32ToNumber, WasmFloat32ToNumber)                       \
  IF_WASM(TFC, WasmFloat64ToNumber, WasmFloat64ToNumber)                       \
  IF_WASM(TFC, WasmI32AtomicWait32, WasmI32AtomicWait32)                       \
  IF_WASM(TFC, WasmI64AtomicWait32, WasmI64AtomicWait32)                       \
  IF_WASM(TFC, JSToWasmLazyDeoptContinuation, SingleParameterOnStack)          \
                                                                               \
  /* WeakMap */                                                                \
  TFJ(WeakMapConstructor, kDontAdaptArgumentsSentinel)                         \
  TFS(WeakMapLookupHashIndex, kTable, kKey)                                    \
  TFJ(WeakMapGet, 1, kReceiver, kKey)                                          \
  TFJ(WeakMapPrototypeHas, 1, kReceiver, kKey)                                 \
  TFJ(WeakMapPrototypeSet, 2, kReceiver, kKey, kValue)                         \
  TFJ(WeakMapPrototypeDelete, 1, kReceiver, kKey)                              \
                                                                               \
  /* WeakSet */                                                                \
  TFJ(WeakSetConstructor, kDontAdaptArgumentsSentinel)                         \
  TFJ(WeakSetPrototypeHas, 1, kReceiver, kKey)                                 \
  TFJ(WeakSetPrototypeAdd, 1, kReceiver, kValue)                               \
  TFJ(WeakSetPrototypeDelete, 1, kReceiver, kValue)                            \
                                                                               \
  /* WeakSet / WeakMap Helpers */                                              \
  TFS(WeakCollectionDelete, kCollection, kKey)                                 \
  TFS(WeakCollectionSet, kCollection, kKey, kValue)                            \
                                                                               \
  /* AsyncGenerator */                                                         \
                                                                               \
  TFS(AsyncGeneratorResolve, kGenerator, kValue, kDone)                        \
  TFS(AsyncGeneratorReject, kGenerator, kValue)                                \
  TFS(AsyncGeneratorYield, kGenerator, kValue, kIsCaught)                      \
  TFS(AsyncGeneratorReturn, kGenerator, kValue, kIsCaught)                     \
  TFS(AsyncGeneratorResumeNext, kGenerator)                                    \
                                                                               \
  /* AsyncGeneratorFunction( p1, p2, ... pn, body ) */                         \
  /* proposal-async-iteration/#sec-asyncgeneratorfunction-constructor */       \
  CPP(AsyncGeneratorFunctionConstructor)                                       \
  /* AsyncGenerator.prototype.next ( value ) */                                \
  /* proposal-async-iteration/#sec-asyncgenerator-prototype-next */            \
  TFJ(AsyncGeneratorPrototypeNext, kDontAdaptArgumentsSentinel)                \
  /* AsyncGenerator.prototype.return ( value ) */                              \
  /* proposal-async-iteration/#sec-asyncgenerator-prototype-return */          \
  TFJ(AsyncGeneratorPrototypeReturn, kDontAdaptArgumentsSentinel)              \
  /* AsyncGenerator.prototype.throw ( exception ) */                           \
  /* proposal-async-iteration/#sec-asyncgenerator-prototype-throw */           \
  TFJ(AsyncGeneratorPrototypeThrow, kDontAdaptArgumentsSentinel)               \
                                                                               \
  /* Await (proposal-async-iteration/#await), with resume behaviour */         \
  /* specific to Async Generators. Internal / Not exposed to JS code. */       \
  TFS(AsyncGeneratorAwaitCaught, kAsyncGeneratorObject, kValue)                \
  TFS(AsyncGeneratorAwaitUncaught, kAsyncGeneratorObject, kValue)              \
  TFJ(AsyncGeneratorAwaitResolveClosure, 1, kReceiver, kValue)                 \
  TFJ(AsyncGeneratorAwaitRejectClosure, 1, kReceiver, kValue)                  \
  TFJ(AsyncGeneratorYieldResolveClosure, 1, kReceiver, kValue)                 \
  TFJ(AsyncGeneratorReturnClosedResolveClosure, 1, kReceiver, kValue)          \
  TFJ(AsyncGeneratorReturnClosedRejectClosure, 1, kReceiver, kValue)           \
  TFJ(AsyncGeneratorReturnResolveClosure, 1, kReceiver, kValue)                \
                                                                               \
  /* Async-from-Sync Iterator */                                               \
                                                                               \
  /* %AsyncFromSyncIteratorPrototype% */                                       \
  /* See tc39.github.io/proposal-async-iteration/ */                           \
  /* #sec-%asyncfromsynciteratorprototype%-object) */                          \
  TFJ(AsyncFromSyncIteratorPrototypeNext, kDontAdaptArgumentsSentinel)         \
  /* #sec-%asyncfromsynciteratorprototype%.throw */                            \
  TFJ(AsyncFromSyncIteratorPrototypeThrow, kDontAdaptArgumentsSentinel)        \
  /* #sec-%asyncfromsynciteratorprototype%.return */                           \
  TFJ(AsyncFromSyncIteratorPrototypeReturn, kDontAdaptArgumentsSentinel)       \
  /* #sec-async-iterator-value-unwrap-functions */                             \
  TFJ(AsyncIteratorValueUnwrap, 1, kReceiver, kValue)                          \
                                                                               \
  /* CEntry */                                                                 \
  ASM(CEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit, Dummy)          \
  ASM(CEntry_Return1_DontSaveFPRegs_ArgvOnStack_BuiltinExit, Dummy)            \
  ASM(CEntry_Return1_DontSaveFPRegs_ArgvInRegister_NoBuiltinExit, Dummy)       \
  ASM(CEntry_Return1_SaveFPRegs_ArgvOnStack_NoBuiltinExit, Dummy)              \
  ASM(CEntry_Return1_SaveFPRegs_ArgvOnStack_BuiltinExit, Dummy)                \
  ASM(CEntry_Return2_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit, Dummy)          \
  ASM(CEntry_Return2_DontSaveFPRegs_ArgvOnStack_BuiltinExit, Dummy)            \
  ASM(CEntry_Return2_DontSaveFPRegs_ArgvInRegister_NoBuiltinExit, Dummy)       \
  ASM(CEntry_Return2_SaveFPRegs_ArgvOnStack_NoBuiltinExit, Dummy)              \
  ASM(CEntry_Return2_SaveFPRegs_ArgvOnStack_BuiltinExit, Dummy)                \
  ASM(DirectCEntry, Dummy)                                                     \
                                                                               \
  /* String helpers */                                                         \
  TFS(StringAdd_CheckNone, kLeft, kRight)                                      \
  TFS(SubString, kString, kFrom, kTo)                                          \
                                                                               \
  /* Miscellaneous */                                                          \
  ASM(StackCheck, Dummy)                                                       \
  ASM(DoubleToI, Dummy)                                                        \
  TFC(GetProperty, GetProperty)                                                \
  TFS(GetPropertyWithReceiver, kObject, kKey, kReceiver, kOnNonExistent)       \
  TFS(SetProperty, kReceiver, kKey, kValue)                                    \
  TFS(SetPropertyInLiteral, kReceiver, kKey, kValue)                           \
  ASM(MemCopyUint8Uint8, CCall)                                                \
  ASM(MemMove, CCall)                                                          \
                                                                               \
  /* Trace */                                                                  \
  CPP(IsTraceCategoryEnabled)                                                  \
  CPP(Trace)                                                                   \
                                                                               \
  /* Weak refs */                                                              \
  CPP(FinalizationRegistryUnregister)                                          \
                                                                               \
  /* Async modules */                                                          \
  TFJ(AsyncModuleEvaluate, kDontAdaptArgumentsSentinel)                        \
                                                                               \
  /* CallAsyncModule* are spec anonymyous functions */                         \
  CPP(CallAsyncModuleFulfilled)                                                \
  CPP(CallAsyncModuleRejected)

#ifdef V8_INTL_SUPPORT
#define BUILTIN_LIST_INTL(CPP, TFJ, TFS)                               \
  /* ecma402 #sec-intl.collator */                                     \
  CPP(CollatorConstructor)                                             \
  /* ecma 402 #sec-collator-compare-functions*/                        \
  CPP(CollatorInternalCompare)                                         \
  /* ecma402 #sec-intl.collator.prototype.compare */                   \
  CPP(CollatorPrototypeCompare)                                        \
  /* ecma402 #sec-intl.collator.supportedlocalesof */                  \
  CPP(CollatorSupportedLocalesOf)                                      \
  /* ecma402 #sec-intl.collator.prototype.resolvedoptions */           \
  CPP(CollatorPrototypeResolvedOptions)                                \
  /* ecma402 #sup-date.prototype.tolocaledatestring */                 \
  CPP(DatePrototypeToLocaleDateString)                                 \
  /* ecma402 #sup-date.prototype.tolocalestring */                     \
  CPP(DatePrototypeToLocaleString)                                     \
  /* ecma402 #sup-date.prototype.tolocaletimestring */                 \
  CPP(DatePrototypeToLocaleTimeString)                                 \
  /* ecma402 #sec-intl.datetimeformat */                               \
  CPP(DateTimeFormatConstructor)                                       \
  /* ecma402 #sec-datetime-format-functions */                         \
  CPP(DateTimeFormatInternalFormat)                                    \
  /* ecma402 #sec-intl.datetimeformat.prototype.format */              \
  CPP(DateTimeFormatPrototypeFormat)                                   \
  /* ecma402 #sec-intl.datetimeformat.prototype.formatrange */         \
  CPP(DateTimeFormatPrototypeFormatRange)                              \
  /* ecma402 #sec-intl.datetimeformat.prototype.formatrangetoparts */  \
  CPP(DateTimeFormatPrototypeFormatRangeToParts)                       \
  /* ecma402 #sec-intl.datetimeformat.prototype.formattoparts */       \
  CPP(DateTimeFormatPrototypeFormatToParts)                            \
  /* ecma402 #sec-intl.datetimeformat.prototype.resolvedoptions */     \
  CPP(DateTimeFormatPrototypeResolvedOptions)                          \
  /* ecma402 #sec-intl.datetimeformat.supportedlocalesof */            \
  CPP(DateTimeFormatSupportedLocalesOf)                                \
  /* ecma402 #sec-Intl.DisplayNames */                                 \
  CPP(DisplayNamesConstructor)                                         \
  /* ecma402 #sec-Intl.DisplayNames.prototype.of */                    \
  CPP(DisplayNamesPrototypeOf)                                         \
  /* ecma402 #sec-Intl.DisplayNames.prototype.resolvedOptions */       \
  CPP(DisplayNamesPrototypeResolvedOptions)                            \
  /* ecma402 #sec-Intl.DisplayNames.supportedLocalesOf */              \
  CPP(DisplayNamesSupportedLocalesOf)                                  \
  /* ecma402 #sec-intl.getcanonicallocales */                          \
  CPP(IntlGetCanonicalLocales)                                         \
  /* ecma402 #sec-intl-listformat-constructor */                       \
  CPP(ListFormatConstructor)                                           \
  /* ecma402 #sec-intl-list-format.prototype.format */                 \
  TFJ(ListFormatPrototypeFormat, kDontAdaptArgumentsSentinel)          \
  /* ecma402 #sec-intl-list-format.prototype.formattoparts */          \
  TFJ(ListFormatPrototypeFormatToParts, kDontAdaptArgumentsSentinel)   \
  /* ecma402 #sec-intl.listformat.prototype.resolvedoptions */         \
  CPP(ListFormatPrototypeResolvedOptions)                              \
  /* ecma402 #sec-intl.ListFormat.supportedlocalesof */                \
  CPP(ListFormatSupportedLocalesOf)                                    \
  /* ecma402 #sec-intl-locale-constructor */                           \
  CPP(LocaleConstructor)                                               \
  /* ecma402 #sec-Intl.Locale.prototype.baseName */                    \
  CPP(LocalePrototypeBaseName)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.calendar */                    \
  CPP(LocalePrototypeCalendar)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.calendars */                   \
  CPP(LocalePrototypeCalendars)                                        \
  /* ecma402 #sec-Intl.Locale.prototype.caseFirst */                   \
  CPP(LocalePrototypeCaseFirst)                                        \
  /* ecma402 #sec-Intl.Locale.prototype.collation */                   \
  CPP(LocalePrototypeCollation)                                        \
  /* ecma402 #sec-Intl.Locale.prototype.collations */                  \
  CPP(LocalePrototypeCollations)                                       \
  /* ecma402 #sec-Intl.Locale.prototype.hourCycle */                   \
  CPP(LocalePrototypeHourCycle)                                        \
  /* ecma402 #sec-Intl.Locale.prototype.hourCycles */                  \
  CPP(LocalePrototypeHourCycles)                                       \
  /* ecma402 #sec-Intl.Locale.prototype.language */                    \
  CPP(LocalePrototypeLanguage)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.maximize */                    \
  CPP(LocalePrototypeMaximize)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.minimize */                    \
  CPP(LocalePrototypeMinimize)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.numeric */                     \
  CPP(LocalePrototypeNumeric)                                          \
  /* ecma402 #sec-Intl.Locale.prototype.numberingSystem */             \
  CPP(LocalePrototypeNumberingSystem)                                  \
  /* ecma402 #sec-Intl.Locale.prototype.numberingSystems */            \
  CPP(LocalePrototypeNumberingSystems)                                 \
  /* ecma402 #sec-Intl.Locale.prototype.region */                      \
  CPP(LocalePrototypeRegion)                                           \
  /* ecma402 #sec-Intl.Locale.prototype.script */                      \
  CPP(LocalePrototypeScript)                                           \
  /* ecma402 #sec-Intl.Locale.prototype.textInfo */                    \
  CPP(LocalePrototypeTextInfo)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.timezones */                   \
  CPP(LocalePrototypeTimeZones)                                        \
  /* ecma402 #sec-Intl.Locale.prototype.toString */                    \
  CPP(LocalePrototypeToString)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.weekInfo */                    \
  CPP(LocalePrototypeWeekInfo)                                         \
  /* ecma402 #sec-intl.numberformat */                                 \
  CPP(NumberFormatConstructor)                                         \
  /* ecma402 #sec-number-format-functions */                           \
  CPP(NumberFormatInternalFormatNumber)                                \
  /* ecma402 #sec-intl.numberformat.prototype.format */                \
  CPP(NumberFormatPrototypeFormatNumber)                               \
  /* ecma402 #sec-intl.numberformat.prototype.formattoparts */         \
  CPP(NumberFormatPrototypeFormatToParts)                              \
  /* ecma402 #sec-intl.numberformat.prototype.resolvedoptions */       \
  CPP(NumberFormatPrototypeResolvedOptions)                            \
  /* ecma402 #sec-intl.numberformat.supportedlocalesof */              \
  CPP(NumberFormatSupportedLocalesOf)                                  \
  /* ecma402 #sec-intl.pluralrules */                                  \
  CPP(PluralRulesConstructor)                                          \
  /* ecma402 #sec-intl.pluralrules.prototype.resolvedoptions */        \
  CPP(PluralRulesPrototypeResolvedOptions)                             \
  /* ecma402 #sec-intl.pluralrules.prototype.select */                 \
  CPP(PluralRulesPrototypeSelect)                                      \
  /* ecma402 #sec-intl.pluralrules.supportedlocalesof */               \
  CPP(PluralRulesSupportedLocalesOf)                                   \
  /* ecma402 #sec-intl.RelativeTimeFormat.constructor */               \
  CPP(RelativeTimeFormatConstructor)                                   \
  /* ecma402 #sec-intl.RelativeTimeFormat.prototype.format */          \
  CPP(RelativeTimeFormatPrototypeFormat)                               \
  /* ecma402 #sec-intl.RelativeTimeFormat.prototype.formatToParts */   \
  CPP(RelativeTimeFormatPrototypeFormatToParts)                        \
  /* ecma402 #sec-intl.RelativeTimeFormat.prototype.resolvedOptions */ \
  CPP(RelativeTimeFormatPrototypeResolvedOptions)                      \
  /* ecma402 #sec-intl.RelativeTimeFormat.supportedlocalesof */        \
  CPP(RelativeTimeFormatSupportedLocalesOf)                            \
  /* ecma402 #sec-Intl.Segmenter */                                    \
  CPP(SegmenterConstructor)                                            \
  /* ecma402 #sec-Intl.Segmenter.prototype.resolvedOptions */          \
  CPP(SegmenterPrototypeResolvedOptions)                               \
  /* ecma402 #sec-Intl.Segmenter.prototype.segment  */                 \
  CPP(SegmenterPrototypeSegment)                                       \
  /* ecma402  #sec-Intl.Segmenter.supportedLocalesOf */                \
  CPP(SegmenterSupportedLocalesOf)                                     \
  /* ecma402 #sec-segment-iterator-prototype-next */                   \
  CPP(SegmentIteratorPrototypeNext)                                    \
  /* ecma402 #sec-%segmentsprototype%.containing */                    \
  CPP(SegmentsPrototypeContaining)                                     \
  /* ecma402 #sec-%segmentsprototype%-@@iterator */                    \
  CPP(SegmentsPrototypeIterator)                                       \
  /* ES #sec-string.prototype.normalize */                             \
  CPP(StringPrototypeNormalizeIntl)                                    \
  /* ecma402 #sup-string.prototype.tolocalelowercase */                \
  CPP(StringPrototypeToLocaleLowerCase)                                \
  /* ecma402 #sup-string.prototype.tolocaleuppercase */                \
  CPP(StringPrototypeToLocaleUpperCase)                                \
  /* ES #sec-string.prototype.tolowercase */                           \
  TFJ(StringPrototypeToLowerCaseIntl, 0, kReceiver)                    \
  /* ES #sec-string.prototype.touppercase */                           \
  CPP(StringPrototypeToUpperCaseIntl)                                  \
  TFS(StringToLowerCaseIntl, kString)                                  \
  CPP(V8BreakIteratorConstructor)                                      \
  CPP(V8BreakIteratorInternalAdoptText)                                \
  CPP(V8BreakIteratorInternalBreakType)                                \
  CPP(V8BreakIteratorInternalCurrent)                                  \
  CPP(V8BreakIteratorInternalFirst)                                    \
  CPP(V8BreakIteratorInternalNext)                                     \
  CPP(V8BreakIteratorPrototypeAdoptText)                               \
  CPP(V8BreakIteratorPrototypeBreakType)                               \
  CPP(V8BreakIteratorPrototypeCurrent)                                 \
  CPP(V8BreakIteratorPrototypeFirst)                                   \
  CPP(V8BreakIteratorPrototypeNext)                                    \
  CPP(V8BreakIteratorPrototypeResolvedOptions)                         \
  CPP(V8BreakIteratorSupportedLocalesOf)
#else
#define BUILTIN_LIST_INTL(CPP, TFJ, TFS)      \
  /* no-op fallback version */                \
  CPP(StringPrototypeNormalize)               \
  /* same as toLowercase; fallback version */ \
  CPP(StringPrototypeToLocaleLowerCase)       \
  /* same as toUppercase; fallback version */ \
  CPP(StringPrototypeToLocaleUpperCase)       \
  /* (obsolete) Unibrow version */            \
  CPP(StringPrototypeToLowerCase)             \
  /* (obsolete) Unibrow version */            \
  CPP(StringPrototypeToUpperCase)
#endif  // V8_INTL_SUPPORT

#define BUILTIN_LIST(CPP, TFJ, TFC, TFS, TFH, BCH, ASM)  \
  BUILTIN_LIST_BASE(CPP, TFJ, TFC, TFS, TFH, ASM)        \
  BUILTIN_LIST_FROM_TORQUE(CPP, TFJ, TFC, TFS, TFH, ASM) \
  BUILTIN_LIST_INTL(CPP, TFJ, TFS)                       \
  BUILTIN_LIST_BYTECODE_HANDLERS(BCH)

// The exception thrown in the following builtins are caught
// internally and result in a promise rejection.
#define BUILTIN_PROMISE_REJECTION_PREDICTION_LIST(V) \
  V(AsyncFromSyncIteratorPrototypeNext)              \
  V(AsyncFromSyncIteratorPrototypeReturn)            \
  V(AsyncFromSyncIteratorPrototypeThrow)             \
  V(AsyncFunctionAwaitCaught)                        \
  V(AsyncFunctionAwaitUncaught)                      \
  V(AsyncGeneratorResolve)                           \
  V(AsyncGeneratorAwaitCaught)                       \
  V(AsyncGeneratorAwaitUncaught)                     \
  V(PromiseAll)                                      \
  V(PromiseAny)                                      \
  V(PromiseConstructor)                              \
  V(PromiseConstructorLazyDeoptContinuation)         \
  V(PromiseFulfillReactionJob)                       \
  V(PromiseRace)                                     \
  V(ResolvePromise)

// The exception thrown in the following builtins are caught internally and will
// not be propagated further or re-thrown
#define BUILTIN_EXCEPTION_CAUGHT_PREDICTION_LIST(V) V(PromiseRejectReactionJob)

#define IGNORE_BUILTIN(...)

#define BUILTIN_LIST_C(V)                                         \
  BUILTIN_LIST(V, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_A(V)                                                      \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, V)

#define BUILTIN_LIST_TFS(V)                                       \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, V, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFJ(V)                                       \
  BUILTIN_LIST(IGNORE_BUILTIN, V, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFC(V)                                       \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, V, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DEFINITIONS_H_
