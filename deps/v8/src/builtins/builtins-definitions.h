// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DEFINITIONS_H_
#define V8_BUILTINS_BUILTINS_DEFINITIONS_H_

#include "builtins-generated/bytecodes-builtins-list.h"

// include generated header
#include "torque-generated/builtin-definitions-tq.h"

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
  TFC(RecordWrite, RecordWrite)                                                \
  TFC(EphemeronKeyBarrier, EphemeronKeyBarrier)                                \
                                                                               \
  /* Adaptor for CPP builtin */                                                \
  TFC(AdaptorWithBuiltinExitFrame, CppBuiltinAdaptor)                          \
                                                                               \
  /* Calls */                                                                  \
  ASM(ArgumentsAdaptorTrampoline, ArgumentsAdaptor)                            \
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
                                                                               \
  /* ES6 section 9.5.12[[Call]] ( thisArgument, argumentsList ) */             \
  TFC(CallProxy, CallTrampoline)                                               \
  ASM(CallVarargs, CallVarargs)                                                \
  TFC(CallWithSpread, CallWithSpread)                                          \
  TFC(CallWithArrayLike, CallWithArrayLike)                                    \
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
  TFC(ConstructWithArrayLike, ConstructWithArrayLike)                          \
  ASM(ConstructForwardVarargs, ConstructForwardVarargs)                        \
  ASM(ConstructFunctionForwardVarargs, ConstructForwardVarargs)                \
  ASM(JSConstructStubGeneric, Dummy)                                           \
  ASM(JSBuiltinsConstructStub, Dummy)                                          \
  TFC(FastNewObject, FastNewObject)                                            \
  TFS(FastNewClosure, kSharedFunctionInfo, kFeedbackCell)                      \
  TFC(FastNewFunctionContextEval, FastNewFunctionContext)                      \
  TFC(FastNewFunctionContextFunction, FastNewFunctionContext)                  \
  TFS(CreateRegExpLiteral, kFeedbackVector, kSlot, kPattern, kFlags)           \
  TFS(CreateEmptyArrayLiteral, kFeedbackVector, kSlot)                         \
  TFS(CreateShallowArrayLiteral, kFeedbackVector, kSlot, kConstantElements)    \
  TFS(CreateShallowObjectLiteral, kFeedbackVector, kSlot,                      \
      kObjectBoilerplateDescription, kFlags)                                   \
  /* ES6 section 9.5.14 [[Construct]] ( argumentsList, newTarget) */           \
  TFC(ConstructProxy, JSTrampoline)                                            \
                                                                               \
  /* Apply and entries */                                                      \
  ASM(JSEntry, Dummy)                                                          \
  ASM(JSConstructEntry, Dummy)                                                 \
  ASM(JSRunMicrotasksEntry, RunMicrotasksEntry)                                \
  ASM(JSEntryTrampoline, Dummy)                                                \
  ASM(JSConstructEntryTrampoline, Dummy)                                       \
  ASM(ResumeGeneratorTrampoline, ResumeGenerator)                              \
                                                                               \
  /* String helpers */                                                         \
  TFC(StringCodePointAt, StringAt)                                             \
  TFC(StringFromCodePointAt, StringAtAsString)                                 \
  TFC(StringEqual, Compare)                                                    \
  TFC(StringGreaterThan, Compare)                                              \
  TFC(StringGreaterThanOrEqual, Compare)                                       \
  TFS(StringIndexOf, kReceiver, kSearchString, kPosition)                      \
  TFC(StringLessThan, Compare)                                                 \
  TFC(StringLessThanOrEqual, Compare)                                          \
  TFC(StringSubstring, StringSubstring)                                        \
                                                                               \
  /* OrderedHashTable helpers */                                               \
  TFS(OrderedHashTableHealIndex, kTable, kIndex)                               \
                                                                               \
  /* Interpreter */                                                            \
  ASM(InterpreterEntryTrampoline, Dummy)                                       \
  ASM(InterpreterPushArgsThenCall, InterpreterPushArgsThenCall)                \
  ASM(InterpreterPushUndefinedAndArgsThenCall, InterpreterPushArgsThenCall)    \
  ASM(InterpreterPushArgsThenCallWithFinalSpread, InterpreterPushArgsThenCall) \
  ASM(InterpreterPushArgsThenConstruct, InterpreterPushArgsThenConstruct)      \
  ASM(InterpreterPushArgsThenConstructArrayFunction,                           \
      InterpreterPushArgsThenConstruct)                                        \
  ASM(InterpreterPushArgsThenConstructWithFinalSpread,                         \
      InterpreterPushArgsThenConstruct)                                        \
  ASM(InterpreterEnterBytecodeAdvance, Dummy)                                  \
  ASM(InterpreterEnterBytecodeDispatch, Dummy)                                 \
  ASM(InterpreterOnStackReplacement, ContextOnly)                              \
                                                                               \
  /* Code life-cycle */                                                        \
  TFC(CompileLazy, JSTrampoline)                                               \
  TFC(CompileLazyDeoptimizedCode, JSTrampoline)                                \
  ASM(InstantiateAsmJs, Dummy)                                                 \
  ASM(NotifyDeoptimized, Dummy)                                                \
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
  TFC(NewArgumentsElements, NewArgumentsElements)                              \
                                                                               \
  /* Debugger */                                                               \
  TFJ(DebugBreakTrampoline, SharedFunctionInfo::kDontAdaptArgumentsSentinel)   \
  ASM(FrameDropperTrampoline, FrameDropperTrampoline)                          \
  ASM(HandleDebuggerStatement, ContextOnly)                                    \
                                                                               \
  /* Type conversions */                                                       \
  TFC(ToObject, TypeConversion)                                                \
  TFC(ToBoolean, TypeConversion)                                               \
  TFC(OrdinaryToPrimitive_Number, TypeConversion)                              \
  TFC(OrdinaryToPrimitive_String, TypeConversion)                              \
  TFC(NonPrimitiveToPrimitive_Default, TypeConversion)                         \
  TFC(NonPrimitiveToPrimitive_Number, TypeConversion)                          \
  TFC(NonPrimitiveToPrimitive_String, TypeConversion)                          \
  TFC(StringToNumber, TypeConversion)                                          \
  TFC(ToName, TypeConversion)                                                  \
  TFC(NonNumberToNumber, TypeConversion)                                       \
  TFC(NonNumberToNumeric, TypeConversion)                                      \
  TFC(ToNumber, TypeConversion)                                                \
  TFC(ToNumberConvertBigInt, TypeConversion)                                   \
  TFC(ToNumeric, TypeConversion)                                               \
  TFC(NumberToString, TypeConversion)                                          \
  TFC(ToInteger, TypeConversion)                                               \
  TFC(ToInteger_TruncateMinusZero, TypeConversion)                             \
  TFC(ToLength, TypeConversion)                                                \
  TFC(Typeof, Typeof)                                                          \
  TFC(GetSuperConstructor, Typeof)                                             \
  TFC(BigIntToI64, BigIntToI64)                                                \
  TFC(BigIntToI32Pair, BigIntToI32Pair)                                        \
  TFC(I64ToBigInt, I64ToBigInt)                                                \
  TFC(I32PairToBigInt, I32PairToBigInt)                                        \
                                                                               \
  /* Type conversions continuations */                                         \
  TFC(ToBooleanLazyDeoptContinuation, TypeConversionStackParameter)            \
                                                                               \
  /* Handlers */                                                               \
  TFH(KeyedLoadIC_PolymorphicName, LoadWithVector)                             \
  TFH(KeyedLoadIC_Slow, LoadWithVector)                                        \
  TFH(KeyedStoreIC_Megamorphic, Store)                                         \
  TFH(KeyedStoreIC_Slow, StoreWithVector)                                      \
  TFH(LoadIC_FunctionPrototype, LoadWithVector)                                \
  TFH(LoadIC_StringLength, LoadWithVector)                                     \
  TFH(LoadIC_StringWrapperLength, LoadWithVector)                              \
  TFH(LoadIC_NoFeedback, Load)                                                 \
  TFH(StoreGlobalIC_Slow, StoreWithVector)                                     \
  TFH(StoreIC_NoFeedback, Store)                                               \
  TFH(StoreInArrayLiteralIC_Slow, StoreWithVector)                             \
  TFH(KeyedLoadIC_SloppyArguments, LoadWithVector)                             \
  TFH(LoadIndexedInterceptorIC, LoadWithVector)                                \
  TFH(KeyedStoreIC_SloppyArguments_Standard, StoreWithVector)                  \
  TFH(KeyedStoreIC_SloppyArguments_GrowNoTransitionHandleCOW, StoreWithVector) \
  TFH(KeyedStoreIC_SloppyArguments_NoTransitionIgnoreOOB, StoreWithVector)     \
  TFH(KeyedStoreIC_SloppyArguments_NoTransitionHandleCOW, StoreWithVector)     \
  TFH(StoreInArrayLiteralIC_Slow_Standard, StoreWithVector)                    \
  TFH(StoreFastElementIC_Standard, StoreWithVector)                            \
  TFH(StoreFastElementIC_GrowNoTransitionHandleCOW, StoreWithVector)           \
  TFH(StoreFastElementIC_NoTransitionIgnoreOOB, StoreWithVector)               \
  TFH(StoreFastElementIC_NoTransitionHandleCOW, StoreWithVector)               \
  TFH(StoreInArrayLiteralIC_Slow_GrowNoTransitionHandleCOW, StoreWithVector)   \
  TFH(StoreInArrayLiteralIC_Slow_NoTransitionIgnoreOOB, StoreWithVector)       \
  TFH(StoreInArrayLiteralIC_Slow_NoTransitionHandleCOW, StoreWithVector)       \
  TFH(KeyedStoreIC_Slow_Standard, StoreWithVector)                             \
  TFH(KeyedStoreIC_Slow_GrowNoTransitionHandleCOW, StoreWithVector)            \
  TFH(KeyedStoreIC_Slow_NoTransitionIgnoreOOB, StoreWithVector)                \
  TFH(KeyedStoreIC_Slow_NoTransitionHandleCOW, StoreWithVector)                \
  TFH(ElementsTransitionAndStore_Standard, StoreTransition)                    \
  TFH(ElementsTransitionAndStore_GrowNoTransitionHandleCOW, StoreTransition)   \
  TFH(ElementsTransitionAndStore_NoTransitionIgnoreOOB, StoreTransition)       \
  TFH(ElementsTransitionAndStore_NoTransitionHandleCOW, StoreTransition)       \
  TFH(KeyedHasIC_PolymorphicName, LoadWithVector)                              \
  TFH(KeyedHasIC_SloppyArguments, LoadWithVector)                              \
  TFH(HasIndexedInterceptorIC, LoadWithVector)                                 \
  TFH(HasIC_Slow, LoadWithVector)                                              \
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
  ASM(InternalArrayConstructor, JSTrampoline)                                  \
  ASM(InternalArrayConstructorImpl, JSTrampoline)                              \
  TFC(InternalArrayNoArgumentConstructor_Packed, ArrayNoArgumentConstructor)   \
  CPP(ArrayConcat)                                                             \
  /* ES6 #sec-array.isarray */                                                 \
  TFJ(ArrayIsArray, 1, kReceiver, kArg)                                        \
  /* ES6 #sec-array.prototype.fill */                                          \
  CPP(ArrayPrototypeFill)                                                      \
  /* ES6 #sec-array.from */                                                    \
  TFJ(ArrayFrom, SharedFunctionInfo::kDontAdaptArgumentsSentinel)              \
  /* ES7 #sec-array.prototype.includes */                                      \
  TFS(ArrayIncludesSmiOrObject, kElements, kSearchElement, kLength,            \
      kFromIndex)                                                              \
  TFS(ArrayIncludesPackedDoubles, kElements, kSearchElement, kLength,          \
      kFromIndex)                                                              \
  TFS(ArrayIncludesHoleyDoubles, kElements, kSearchElement, kLength,           \
      kFromIndex)                                                              \
  TFJ(ArrayIncludes, SharedFunctionInfo::kDontAdaptArgumentsSentinel)          \
  /* ES6 #sec-array.prototype.indexof */                                       \
  TFS(ArrayIndexOfSmiOrObject, kElements, kSearchElement, kLength, kFromIndex) \
  TFS(ArrayIndexOfPackedDoubles, kElements, kSearchElement, kLength,           \
      kFromIndex)                                                              \
  TFS(ArrayIndexOfHoleyDoubles, kElements, kSearchElement, kLength,            \
      kFromIndex)                                                              \
  TFJ(ArrayIndexOf, SharedFunctionInfo::kDontAdaptArgumentsSentinel)           \
  /* ES6 #sec-array.prototype.pop */                                           \
  CPP(ArrayPop)                                                                \
  TFJ(ArrayPrototypePop, SharedFunctionInfo::kDontAdaptArgumentsSentinel)      \
  /* ES6 #sec-array.prototype.push */                                          \
  CPP(ArrayPush)                                                               \
  TFJ(ArrayPrototypePush, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
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
  TFJ(ArrayPrototypeFlat, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
  /* https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatMap */   \
  TFJ(ArrayPrototypeFlatMap, SharedFunctionInfo::kDontAdaptArgumentsSentinel)  \
                                                                               \
  /* ArrayBuffer */                                                            \
  /* ES #sec-arraybuffer-constructor */                                        \
  CPP(ArrayBufferConstructor)                                                  \
  CPP(ArrayBufferConstructor_DoNotInitialize)                                  \
  CPP(ArrayBufferPrototypeGetByteLength)                                       \
  CPP(ArrayBufferIsView)                                                       \
  CPP(ArrayBufferPrototypeSlice)                                               \
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
  /* Boolean */                                                                \
  /* ES6 #sec-boolean.prototype.tostring */                                    \
  TFJ(BooleanPrototypeToString, 0, kReceiver)                                  \
  /* ES6 #sec-boolean.prototype.valueof */                                     \
  TFJ(BooleanPrototypeValueOf, 0, kReceiver)                                   \
                                                                               \
  /* CallSite */                                                               \
  CPP(CallSitePrototypeGetColumnNumber)                                        \
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
  TFJ(FastConsoleAssert, SharedFunctionInfo::kDontAdaptArgumentsSentinel)      \
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
  CPP(MakeError)                                                               \
  CPP(MakeRangeError)                                                          \
  CPP(MakeSyntaxError)                                                         \
  CPP(MakeTypeError)                                                           \
  CPP(MakeURIError)                                                            \
                                                                               \
  /* ExtrasUtils */                                                            \
  CPP(ExtrasUtilsUncurryThis)                                                  \
  CPP(ExtrasUtilsCallReflectApply)                                             \
                                                                               \
  /* Function */                                                               \
  CPP(FunctionConstructor)                                                     \
  ASM(FunctionPrototypeApply, Dummy)                                           \
  CPP(FunctionPrototypeBind)                                                   \
  /* ES6 #sec-function.prototype.bind */                                       \
  TFJ(FastFunctionPrototypeBind,                                               \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  ASM(FunctionPrototypeCall, Dummy)                                            \
  /* ES6 #sec-function.prototype-@@hasinstance */                              \
  TFJ(FunctionPrototypeHasInstance, 1, kReceiver, kV)                          \
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
  TFJ(GeneratorPrototypeNext, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  /* ES6 #sec-generator.prototype.return */                                    \
  TFJ(GeneratorPrototypeReturn,                                                \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-generator.prototype.throw */                                     \
  TFJ(GeneratorPrototypeThrow,                                                 \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  CPP(AsyncFunctionConstructor)                                                \
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
  TFH(LoadICTrampoline_Megamorphic, Load)                                      \
  TFH(KeyedLoadIC, LoadWithVector)                                             \
  TFH(KeyedLoadIC_Megamorphic, LoadWithVector)                                 \
  TFH(KeyedLoadICTrampoline, Load)                                             \
  TFH(KeyedLoadICTrampoline_Megamorphic, Load)                                 \
  TFH(StoreGlobalIC, StoreGlobalWithVector)                                    \
  TFH(StoreGlobalICTrampoline, StoreGlobal)                                    \
  TFH(StoreIC, StoreWithVector)                                                \
  TFH(StoreICTrampoline, Store)                                                \
  TFH(KeyedStoreIC, StoreWithVector)                                           \
  TFH(KeyedStoreICTrampoline, Store)                                           \
  TFH(StoreInArrayLiteralIC, StoreWithVector)                                  \
  TFH(LoadGlobalIC, LoadGlobalWithVector)                                      \
  TFH(LoadGlobalICInsideTypeof, LoadGlobalWithVector)                          \
  TFH(LoadGlobalICTrampoline, LoadGlobal)                                      \
  TFH(LoadGlobalICInsideTypeofTrampoline, LoadGlobal)                          \
  TFH(CloneObjectIC, CloneObjectWithVector)                                    \
  TFH(CloneObjectIC_Slow, CloneObjectWithVector)                               \
  TFH(KeyedHasIC, LoadWithVector)                                              \
  TFH(KeyedHasIC_Megamorphic, LoadWithVector)                                  \
                                                                               \
  /* IterableToList */                                                         \
  /* ES #sec-iterabletolist */                                                 \
  TFS(IterableToList, kIterable, kIteratorFn)                                  \
  TFS(IterableToListWithSymbolLookup, kIterable)                               \
  TFS(IterableToListMayPreserveHoles, kIterable, kIteratorFn)                  \
  TFS(IterableToFixedArrayForWasm, kIterable, kExpectedLength)                 \
                                                                               \
  /* #sec-createstringlistfromiterable */                                      \
  TFS(StringListFromIterable, kIterable)                                       \
                                                                               \
  /* Map */                                                                    \
  TFS(FindOrderedHashMapEntry, kTable, kKey)                                   \
  TFJ(MapConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)         \
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
  TFJ(MapPrototypeForEach, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
  /* ES #sec-map.prototype.keys */                                             \
  TFJ(MapPrototypeKeys, 0, kReceiver)                                          \
  /* ES #sec-map.prototype.values */                                           \
  TFJ(MapPrototypeValues, 0, kReceiver)                                        \
  /* ES #sec-%mapiteratorprototype%.next */                                    \
  TFJ(MapIteratorPrototypeNext, 0, kReceiver)                                  \
  TFS(MapIteratorToList, kSource)                                              \
                                                                               \
  /* Math */                                                                   \
  /* ES6 #sec-math.abs */                                                      \
  TFJ(MathAbs, 1, kReceiver, kX)                                               \
  /* ES6 #sec-math.ceil */                                                     \
  TFJ(MathCeil, 1, kReceiver, kX)                                              \
  /* ES6 #sec-math.floor */                                                    \
  TFJ(MathFloor, 1, kReceiver, kX)                                             \
  /* ES6 #sec-math.imul */                                                     \
  TFJ(MathImul, 2, kReceiver, kX, kY)                                          \
  /* ES6 #sec-math.max */                                                      \
  TFJ(MathMax, SharedFunctionInfo::kDontAdaptArgumentsSentinel)                \
  /* ES6 #sec-math.min */                                                      \
  TFJ(MathMin, SharedFunctionInfo::kDontAdaptArgumentsSentinel)                \
  /* ES6 #sec-math.pow */                                                      \
  TFJ(MathPow, 2, kReceiver, kBase, kExponent)                                 \
  /* ES6 #sec-math.random */                                                   \
  TFJ(MathRandom, 0, kReceiver)                                                \
  /* ES6 #sec-math.round */                                                    \
  TFJ(MathRound, 1, kReceiver, kX)                                             \
  /* ES6 #sec-math.trunc */                                                    \
  TFJ(MathTrunc, 1, kReceiver, kX)                                             \
                                                                               \
  /* Number */                                                                 \
  TFC(AllocateHeapNumber, AllocateHeapNumber)                                  \
  /* ES #sec-number-constructor */                                             \
  TFJ(NumberConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)      \
  /* ES6 #sec-number.isfinite */                                               \
  TFJ(NumberIsFinite, 1, kReceiver, kNumber)                                   \
  /* ES6 #sec-number.isinteger */                                              \
  TFJ(NumberIsInteger, 1, kReceiver, kNumber)                                  \
  /* ES6 #sec-number.isnan */                                                  \
  TFJ(NumberIsNaN, 1, kReceiver, kNumber)                                      \
  /* ES6 #sec-number.issafeinteger */                                          \
  TFJ(NumberIsSafeInteger, 1, kReceiver, kNumber)                              \
  /* ES6 #sec-number.parsefloat */                                             \
  TFJ(NumberParseFloat, 1, kReceiver, kString)                                 \
  /* ES6 #sec-number.parseint */                                               \
  TFJ(NumberParseInt, 2, kReceiver, kString, kRadix)                           \
  TFS(ParseInt, kString, kRadix)                                               \
  CPP(NumberPrototypeToExponential)                                            \
  CPP(NumberPrototypeToFixed)                                                  \
  CPP(NumberPrototypeToLocaleString)                                           \
  CPP(NumberPrototypeToPrecision)                                              \
  CPP(NumberPrototypeToString)                                                 \
  /* ES6 #sec-number.prototype.valueof */                                      \
  TFJ(NumberPrototypeValueOf, 0, kReceiver)                                    \
  TFC(Add, BinaryOp)                                                           \
  TFC(Subtract, BinaryOp)                                                      \
  TFC(Multiply, BinaryOp)                                                      \
  TFC(Divide, BinaryOp)                                                        \
  TFC(Modulus, BinaryOp)                                                       \
  TFC(Exponentiate, BinaryOp)                                                  \
  TFC(BitwiseAnd, BinaryOp)                                                    \
  TFC(BitwiseOr, BinaryOp)                                                     \
  TFC(BitwiseXor, BinaryOp)                                                    \
  TFC(ShiftLeft, BinaryOp)                                                     \
  TFC(ShiftRight, BinaryOp)                                                    \
  TFC(ShiftRightLogical, BinaryOp)                                             \
  TFC(LessThan, Compare)                                                       \
  TFC(LessThanOrEqual, Compare)                                                \
  TFC(GreaterThan, Compare)                                                    \
  TFC(GreaterThanOrEqual, Compare)                                             \
  TFC(Equal, Compare)                                                          \
  TFC(SameValue, Compare)                                                      \
  TFC(SameValueNumbersOnly, Compare)                                           \
  TFC(StrictEqual, Compare)                                                    \
  TFS(BitwiseNot, kValue)                                                      \
  TFS(Decrement, kValue)                                                       \
  TFS(Increment, kValue)                                                       \
  TFS(Negate, kValue)                                                          \
                                                                               \
  /* Object */                                                                 \
  /* ES #sec-object-constructor */                                             \
  TFJ(ObjectConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)      \
  TFJ(ObjectAssign, SharedFunctionInfo::kDontAdaptArgumentsSentinel)           \
  /* ES #sec-object.create */                                                  \
  TFJ(ObjectCreate, SharedFunctionInfo::kDontAdaptArgumentsSentinel)           \
  TFS(CreateObjectWithoutProperties, kPrototypeArg)                            \
  CPP(ObjectDefineGetter)                                                      \
  CPP(ObjectDefineProperties)                                                  \
  CPP(ObjectDefineProperty)                                                    \
  CPP(ObjectDefineSetter)                                                      \
  TFJ(ObjectEntries, 1, kReceiver, kObject)                                    \
  CPP(ObjectFreeze)                                                            \
  TFJ(ObjectGetOwnPropertyDescriptor,                                          \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  CPP(ObjectGetOwnPropertyDescriptors)                                         \
  TFJ(ObjectGetOwnPropertyNames, 1, kReceiver, kObject)                        \
  CPP(ObjectGetOwnPropertySymbols)                                             \
  TFJ(ObjectIs, 2, kReceiver, kLeft, kRight)                                   \
  CPP(ObjectIsFrozen)                                                          \
  CPP(ObjectIsSealed)                                                          \
  TFJ(ObjectKeys, 1, kReceiver, kObject)                                       \
  CPP(ObjectLookupGetter)                                                      \
  CPP(ObjectLookupSetter)                                                      \
  /* ES6 #sec-object.prototype.tostring */                                     \
  TFJ(ObjectPrototypeToString, 0, kReceiver)                                   \
  /* ES6 #sec-object.prototype.valueof */                                      \
  TFJ(ObjectPrototypeValueOf, 0, kReceiver)                                    \
  /* ES6 #sec-object.prototype.hasownproperty */                               \
  TFJ(ObjectPrototypeHasOwnProperty, 1, kReceiver, kKey)                       \
  TFJ(ObjectPrototypeIsPrototypeOf, 1, kReceiver, kValue)                      \
  CPP(ObjectPrototypePropertyIsEnumerable)                                     \
  CPP(ObjectPrototypeGetProto)                                                 \
  CPP(ObjectPrototypeSetProto)                                                 \
  /* ES #sec-object.prototype.tolocalestring */                                \
  TFJ(ObjectPrototypeToLocaleString, 0, kReceiver)                             \
  CPP(ObjectSeal)                                                              \
  TFS(ObjectToString, kReceiver)                                               \
  TFJ(ObjectValues, 1, kReceiver, kObject)                                     \
                                                                               \
  /* instanceof */                                                             \
  TFC(OrdinaryHasInstance, Compare)                                            \
  TFC(InstanceOf, Compare)                                                     \
                                                                               \
  /* for-in */                                                                 \
  TFS(ForInEnumerate, kReceiver)                                               \
  TFS(ForInFilter, kKey, kObject)                                              \
                                                                               \
  /* Promise */                                                                \
  /* ES #sec-fulfillpromise */                                                 \
  TFS(FulfillPromise, kPromise, kValue)                                        \
  /* ES #sec-rejectpromise */                                                  \
  TFS(RejectPromise, kPromise, kReason, kDebugEvent)                           \
  /* ES #sec-promise-resolve-functions */                                      \
  /* Starting at step 6 of "Promise Resolve Functions" */                      \
  TFS(ResolvePromise, kPromise, kResolution)                                   \
  /* ES #sec-promise-reject-functions */                                       \
  TFJ(PromiseCapabilityDefaultReject, 1, kReceiver, kReason)                   \
  /* ES #sec-promise-resolve-functions */                                      \
  TFJ(PromiseCapabilityDefaultResolve, 1, kReceiver, kResolution)              \
  /* ES6 #sec-getcapabilitiesexecutor-functions */                             \
  TFJ(PromiseGetCapabilitiesExecutor, 2, kReceiver, kResolve, kReject)         \
  /* ES6 #sec-newpromisecapability */                                          \
  TFS(NewPromiseCapability, kConstructor, kDebugEvent)                         \
  TFJ(PromiseConstructorLazyDeoptContinuation, 4, kReceiver, kPromise,         \
      kReject, kException, kResult)                                            \
  /* ES6 #sec-promise-executor */                                              \
  TFJ(PromiseConstructor, 1, kReceiver, kExecutor)                             \
  CPP(IsPromise)                                                               \
  /* ES #sec-promise.prototype.then */                                         \
  TFJ(PromisePrototypeThen, 2, kReceiver, kOnFulfilled, kOnRejected)           \
  /* ES #sec-performpromisethen */                                             \
  TFS(PerformPromiseThen, kPromise, kOnFulfilled, kOnRejected, kResultPromise) \
  /* ES #sec-promise.prototype.catch */                                        \
  TFJ(PromisePrototypeCatch, 1, kReceiver, kOnRejected)                        \
  /* ES #sec-promisereactionjob */                                             \
  TFS(PromiseRejectReactionJob, kReason, kHandler, kPromiseOrCapability)       \
  TFS(PromiseFulfillReactionJob, kValue, kHandler, kPromiseOrCapability)       \
  /* ES #sec-promiseresolvethenablejob */                                      \
  TFS(PromiseResolveThenableJob, kPromiseToResolve, kThenable, kThen)          \
  /* ES #sec-promise.resolve */                                                \
  TFJ(PromiseResolveTrampoline, 1, kReceiver, kValue)                          \
  /* ES #sec-promise-resolve */                                                \
  TFS(PromiseResolve, kConstructor, kValue)                                    \
  /* ES #sec-promise.reject */                                                 \
  TFJ(PromiseReject, 1, kReceiver, kReason)                                    \
  TFJ(PromisePrototypeFinally, 1, kReceiver, kOnFinally)                       \
  TFJ(PromiseThenFinally, 1, kReceiver, kValue)                                \
  TFJ(PromiseCatchFinally, 1, kReceiver, kReason)                              \
  TFJ(PromiseValueThunkFinally, 0, kReceiver)                                  \
  TFJ(PromiseThrowerFinally, 0, kReceiver)                                     \
  /* ES #sec-promise.all */                                                    \
  TFJ(PromiseAll, 1, kReceiver, kIterable)                                     \
  TFJ(PromiseAllResolveElementClosure, 1, kReceiver, kValue)                   \
  /* ES #sec-promise.race */                                                   \
  TFJ(PromiseRace, 1, kReceiver, kIterable)                                    \
  /* ES #sec-promise.allsettled */                                             \
  TFJ(PromiseAllSettled, 1, kReceiver, kIterable)                              \
  TFJ(PromiseAllSettledResolveElementClosure, 1, kReceiver, kValue)            \
  TFJ(PromiseAllSettledRejectElementClosure, 1, kReceiver, kValue)             \
  /* V8 Extras: v8.createPromise(parent) */                                    \
  TFJ(PromiseInternalConstructor, 1, kReceiver, kParent)                       \
  /* V8 Extras: v8.rejectPromise(promise, reason) */                           \
  TFJ(PromiseInternalReject, 2, kReceiver, kPromise, kReason)                  \
  /* V8 Extras: v8.resolvePromise(promise, resolution) */                      \
  TFJ(PromiseInternalResolve, 2, kReceiver, kPromise, kResolution)             \
                                                                               \
  /* Reflect */                                                                \
  ASM(ReflectApply, Dummy)                                                     \
  ASM(ReflectConstruct, Dummy)                                                 \
  CPP(ReflectDefineProperty)                                                   \
  CPP(ReflectGetOwnPropertyDescriptor)                                         \
  TFJ(ReflectHas, 2, kReceiver, kTarget, kKey)                                 \
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
                                                                               \
  /* Set */                                                                    \
  TFJ(SetConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)         \
  TFJ(SetPrototypeHas, 1, kReceiver, kKey)                                     \
  TFJ(SetPrototypeAdd, 1, kReceiver, kKey)                                     \
  TFJ(SetPrototypeDelete, 1, kReceiver, kKey)                                  \
  CPP(SetPrototypeClear)                                                       \
  /* ES #sec-set.prototype.entries */                                          \
  TFJ(SetPrototypeEntries, 0, kReceiver)                                       \
  /* ES #sec-get-set.prototype.size */                                         \
  TFJ(SetPrototypeGetSize, 0, kReceiver)                                       \
  /* ES #sec-set.prototype.foreach */                                          \
  TFJ(SetPrototypeForEach, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
  /* ES #sec-set.prototype.values */                                           \
  TFJ(SetPrototypeValues, 0, kReceiver)                                        \
  /* ES #sec-%setiteratorprototype%.next */                                    \
  TFJ(SetIteratorPrototypeNext, 0, kReceiver)                                  \
  TFS(SetOrSetIteratorToList, kSource)                                         \
                                                                               \
  /* SharedArrayBuffer */                                                      \
  CPP(SharedArrayBufferPrototypeGetByteLength)                                 \
  CPP(SharedArrayBufferPrototypeSlice)                                         \
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
  CPP(AtomicsWake)                                                             \
                                                                               \
  /* String */                                                                 \
  /* ES #sec-string.fromcodepoint */                                           \
  CPP(StringFromCodePoint)                                                     \
  /* ES6 #sec-string.fromcharcode */                                           \
  TFJ(StringFromCharCode, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
  /* ES6 #sec-string.prototype.includes */                                     \
  TFJ(StringPrototypeIncludes,                                                 \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-string.prototype.indexof */                                      \
  TFJ(StringPrototypeIndexOf, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  /* ES6 #sec-string.prototype.lastindexof */                                  \
  CPP(StringPrototypeLastIndexOf)                                              \
  /* ES6 #sec-string.prototype.match */                                        \
  TFJ(StringPrototypeMatch, 1, kReceiver, kRegexp)                             \
  /* ES #sec-string.prototype.matchAll */                                      \
  TFJ(StringPrototypeMatchAll, 1, kReceiver, kRegexp)                          \
  /* ES6 #sec-string.prototype.localecompare */                                \
  CPP(StringPrototypeLocaleCompare)                                            \
  /* ES6 #sec-string.prototype.replace */                                      \
  TFJ(StringPrototypeReplace, 2, kReceiver, kSearch, kReplace)                 \
  /* ES6 #sec-string.prototype.search */                                       \
  TFJ(StringPrototypeSearch, 1, kReceiver, kRegexp)                            \
  /* ES6 #sec-string.prototype.split */                                        \
  TFJ(StringPrototypeSplit, SharedFunctionInfo::kDontAdaptArgumentsSentinel)   \
  /* ES6 #sec-string.prototype.substr */                                       \
  TFJ(StringPrototypeSubstr, SharedFunctionInfo::kDontAdaptArgumentsSentinel)  \
  TFJ(StringPrototypeTrim, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
  TFJ(StringPrototypeTrimEnd, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  TFJ(StringPrototypeTrimStart,                                                \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
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
  /* ES #sec-symbol.prototype.description */                                   \
  TFJ(SymbolPrototypeDescriptionGetter, 0, kReceiver)                          \
  /* ES6 #sec-symbol.prototype-@@toprimitive */                                \
  TFJ(SymbolPrototypeToPrimitive, 1, kReceiver, kHint)                         \
  /* ES6 #sec-symbol.prototype.tostring */                                     \
  TFJ(SymbolPrototypeToString, 0, kReceiver)                                   \
  /* ES6 #sec-symbol.prototype.valueof */                                      \
  TFJ(SymbolPrototypeValueOf, 0, kReceiver)                                    \
                                                                               \
  /* TypedArray */                                                             \
  /* ES #sec-typedarray-constructors */                                        \
  TFJ(TypedArrayBaseConstructor, 0, kReceiver)                                 \
  TFJ(GenericLazyDeoptContinuation, 1, kReceiver, kResult)                     \
  TFJ(TypedArrayConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)  \
  CPP(TypedArrayPrototypeBuffer)                                               \
  /* ES6 #sec-get-%typedarray%.prototype.bytelength */                         \
  TFJ(TypedArrayPrototypeByteLength, 0, kReceiver)                             \
  /* ES6 #sec-get-%typedarray%.prototype.byteoffset */                         \
  TFJ(TypedArrayPrototypeByteOffset, 0, kReceiver)                             \
  /* ES6 #sec-get-%typedarray%.prototype.length */                             \
  TFJ(TypedArrayPrototypeLength, 0, kReceiver)                                 \
  /* ES6 #sec-%typedarray%.prototype.entries */                                \
  TFJ(TypedArrayPrototypeEntries, 0, kReceiver)                                \
  /* ES6 #sec-%typedarray%.prototype.keys */                                   \
  TFJ(TypedArrayPrototypeKeys, 0, kReceiver)                                   \
  /* ES6 #sec-%typedarray%.prototype.values */                                 \
  TFJ(TypedArrayPrototypeValues, 0, kReceiver)                                 \
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
  /* ES6 %TypedArray%.prototype.set */                                         \
  TFJ(TypedArrayPrototypeSet, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  /* ES6 #sec-get-%typedarray%.prototype-@@tostringtag */                      \
  TFJ(TypedArrayPrototypeToStringTag, 0, kReceiver)                            \
  /* ES6 %TypedArray%.prototype.map */                                         \
  TFJ(TypedArrayPrototypeMap, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  /* ES6 %TypedArray%.of */                                                    \
  TFJ(TypedArrayOf, SharedFunctionInfo::kDontAdaptArgumentsSentinel)           \
  /* ES6 %TypedArray%.from */                                                  \
  TFJ(TypedArrayFrom, SharedFunctionInfo::kDontAdaptArgumentsSentinel)         \
                                                                               \
  /* Wasm */                                                                   \
  ASM(WasmCompileLazy, Dummy)                                                  \
  TFC(WasmAllocateHeapNumber, AllocateHeapNumber)                              \
  TFC(WasmAtomicNotify, WasmAtomicNotify)                                      \
  TFC(WasmI32AtomicWait, WasmI32AtomicWait)                                    \
  TFC(WasmI64AtomicWait, WasmI64AtomicWait)                                    \
  TFC(WasmMemoryGrow, WasmMemoryGrow)                                          \
  TFC(WasmTableGet, WasmTableGet)                                              \
  TFC(WasmTableSet, WasmTableSet)                                              \
  TFC(WasmRecordWrite, RecordWrite)                                            \
  TFC(WasmStackGuard, NoContext)                                               \
  TFC(WasmStackOverflow, NoContext)                                            \
  TFC(WasmToNumber, TypeConversion)                                            \
  TFC(WasmThrow, WasmThrow)                                                    \
  TFC(WasmRethrow, WasmThrow)                                                  \
  TFS(ThrowWasmTrapUnreachable)                                                \
  TFS(ThrowWasmTrapMemOutOfBounds)                                             \
  TFS(ThrowWasmTrapUnalignedAccess)                                            \
  TFS(ThrowWasmTrapDivByZero)                                                  \
  TFS(ThrowWasmTrapDivUnrepresentable)                                         \
  TFS(ThrowWasmTrapRemByZero)                                                  \
  TFS(ThrowWasmTrapFloatUnrepresentable)                                       \
  TFS(ThrowWasmTrapFuncInvalid)                                                \
  TFS(ThrowWasmTrapFuncSigMismatch)                                            \
  TFS(ThrowWasmTrapDataSegmentDropped)                                         \
  TFS(ThrowWasmTrapElemSegmentDropped)                                         \
  TFS(ThrowWasmTrapTableOutOfBounds)                                           \
  TFC(WasmI64ToBigInt, I64ToBigInt)                                            \
  TFC(WasmI32PairToBigInt, I32PairToBigInt)                                    \
  TFC(WasmBigIntToI64, BigIntToI64)                                            \
  TFC(WasmBigIntToI32Pair, BigIntToI32Pair)                                    \
                                                                               \
  /* WeakMap */                                                                \
  TFJ(WeakMapConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
  TFS(WeakMapLookupHashIndex, kTable, kKey)                                    \
  TFJ(WeakMapGet, 1, kReceiver, kKey)                                          \
  TFJ(WeakMapPrototypeHas, 1, kReceiver, kKey)                                 \
  TFJ(WeakMapPrototypeSet, 2, kReceiver, kKey, kValue)                         \
  TFJ(WeakMapPrototypeDelete, 1, kReceiver, kKey)                              \
                                                                               \
  /* WeakSet */                                                                \
  TFJ(WeakSetConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
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
  TFJ(AsyncGeneratorPrototypeNext,                                             \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* AsyncGenerator.prototype.return ( value ) */                              \
  /* proposal-async-iteration/#sec-asyncgenerator-prototype-return */          \
  TFJ(AsyncGeneratorPrototypeReturn,                                           \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* AsyncGenerator.prototype.throw ( exception ) */                           \
  /* proposal-async-iteration/#sec-asyncgenerator-prototype-throw */           \
  TFJ(AsyncGeneratorPrototypeThrow,                                            \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
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
  TFJ(AsyncFromSyncIteratorPrototypeNext, 1, kReceiver, kValue)                \
  /* #sec-%asyncfromsynciteratorprototype%.throw */                            \
  TFJ(AsyncFromSyncIteratorPrototypeThrow, 1, kReceiver, kReason)              \
  /* #sec-%asyncfromsynciteratorprototype%.return */                           \
  TFJ(AsyncFromSyncIteratorPrototypeReturn, 1, kReceiver, kValue)              \
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
  CPP(FinalizationGroupCleanupIteratorNext)                                    \
  CPP(FinalizationGroupCleanupSome)                                            \
  CPP(FinalizationGroupConstructor)                                            \
  CPP(FinalizationGroupRegister)                                               \
  CPP(FinalizationGroupUnregister)                                             \
  CPP(WeakRefConstructor)                                                      \
  CPP(WeakRefDeref)                                                            \
                                                                               \
  /* Async modules */                                                          \
  TFJ(AsyncModuleEvaluate, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
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
  /* ecma402 #sec-intl.getcanonicallocales */                          \
  CPP(IntlGetCanonicalLocales)                                         \
  /* ecma402 #sec-intl-listformat-constructor */                       \
  CPP(ListFormatConstructor)                                           \
  /* ecma402 #sec-intl-list-format.prototype.format */                 \
  TFJ(ListFormatPrototypeFormat,                                       \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                 \
  /* ecma402 #sec-intl-list-format.prototype.formattoparts */          \
  TFJ(ListFormatPrototypeFormatToParts,                                \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                 \
  /* ecma402 #sec-intl.listformat.prototype.resolvedoptions */         \
  CPP(ListFormatPrototypeResolvedOptions)                              \
  /* ecma402 #sec-intl.ListFormat.supportedlocalesof */                \
  CPP(ListFormatSupportedLocalesOf)                                    \
  /* ecma402 #sec-intl-locale-constructor */                           \
  CPP(LocaleConstructor)                                               \
  CPP(LocalePrototypeBaseName)                                         \
  CPP(LocalePrototypeCalendar)                                         \
  CPP(LocalePrototypeCaseFirst)                                        \
  CPP(LocalePrototypeCollation)                                        \
  CPP(LocalePrototypeHourCycle)                                        \
  CPP(LocalePrototypeLanguage)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.maximize */                    \
  CPP(LocalePrototypeMaximize)                                         \
  /* ecma402 #sec-Intl.Locale.prototype.minimize */                    \
  CPP(LocalePrototypeMinimize)                                         \
  CPP(LocalePrototypeNumeric)                                          \
  CPP(LocalePrototypeNumberingSystem)                                  \
  CPP(LocalePrototypeRegion)                                           \
  CPP(LocalePrototypeScript)                                           \
  CPP(LocalePrototypeToString)                                         \
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
  /* ecma402 #sec-segment-iterator-prototype-breakType */              \
  CPP(SegmentIteratorPrototypeBreakType)                               \
  /* ecma402 #sec-segment-iterator-prototype-following */              \
  CPP(SegmentIteratorPrototypeFollowing)                               \
  /* ecma402 #sec-segment-iterator-prototype-preceding */              \
  CPP(SegmentIteratorPrototypePreceding)                               \
  /* ecma402 #sec-segment-iterator-prototype-index */                  \
  CPP(SegmentIteratorPrototypeIndex)                                   \
  /* ecma402 #sec-segment-iterator-prototype-next */                   \
  CPP(SegmentIteratorPrototypeNext)                                    \
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
  V(PromiseConstructor)                              \
  V(PromiseConstructorLazyDeoptContinuation)         \
  V(PromiseFulfillReactionJob)                       \
  V(PromiseRace)                                     \
  V(ResolvePromise)

// Convenience macro listing all wasm runtime stubs. Note that the first few
// elements of the list coincide with {compiler::TrapId}, order matters.
#define WASM_RUNTIME_STUB_LIST(V, VTRAP) \
  FOREACH_WASM_TRAPREASON(VTRAP)         \
  V(WasmCompileLazy)                     \
  V(WasmAllocateHeapNumber)              \
  V(WasmAtomicNotify)                    \
  V(WasmI32AtomicWait)                   \
  V(WasmI64AtomicWait)                   \
  V(WasmMemoryGrow)                      \
  V(WasmTableGet)                        \
  V(WasmTableSet)                        \
  V(WasmRecordWrite)                     \
  V(WasmStackGuard)                      \
  V(WasmStackOverflow)                   \
  V(WasmToNumber)                        \
  V(WasmThrow)                           \
  V(WasmRethrow)                         \
  V(DoubleToI)                           \
  V(WasmI64ToBigInt)                     \
  V(WasmI32PairToBigInt)                 \
  V(WasmBigIntToI64)                     \
  V(WasmBigIntToI32Pair)

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
