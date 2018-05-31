// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_DEFINITIONS_H_
#define V8_BUILTINS_BUILTINS_DEFINITIONS_H_

namespace v8 {
namespace internal {

// CPP: Builtin in C++. Entered via BUILTIN_EXIT frame.
//      Args: name
// API: Builtin in C++ for API callbacks. Entered via EXIT frame.
//      Args: name
// TFJ: Builtin in Turbofan, with JS linkage (callable as Javascript function).
//      Args: name, arguments count, explicit argument names...
// TFS: Builtin in Turbofan, with CodeStub linkage.
//      Args: name, explicit argument names...
// TFC: Builtin in Turbofan, with CodeStub linkage and custom descriptor.
//      Args: name, interface descriptor, return_size
// TFH: Handlers in Turbofan, with CodeStub linkage.
//      Args: name, interface descriptor
// ASM: Builtin in platform-dependent assembly.
//      Args: name

#define BUILTIN_LIST_BASE(CPP, API, TFJ, TFC, TFS, TFH, ASM)                   \
  /* GC write barrirer */                                                      \
  TFC(RecordWrite, RecordWrite, 1)                                             \
                                                                               \
  /* Adaptors for CPP/API builtin */                                           \
  ASM(AdaptorWithExitFrame)                                                    \
  ASM(AdaptorWithBuiltinExitFrame)                                             \
                                                                               \
  /* Calls */                                                                  \
  ASM(ArgumentsAdaptorTrampoline)                                              \
  /* ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList) */              \
  ASM(CallFunction_ReceiverIsNullOrUndefined)                                  \
  ASM(CallFunction_ReceiverIsNotNullOrUndefined)                               \
  ASM(CallFunction_ReceiverIsAny)                                              \
  /* ES6 section 9.4.1.1 [[Call]] ( thisArgument, argumentsList) */            \
  ASM(CallBoundFunction)                                                       \
  /* ES6 section 7.3.12 Call(F, V, [argumentsList]) */                         \
  ASM(Call_ReceiverIsNullOrUndefined)                                          \
  ASM(Call_ReceiverIsNotNullOrUndefined)                                       \
  ASM(Call_ReceiverIsAny)                                                      \
                                                                               \
  /* ES6 section 9.5.12[[Call]] ( thisArgument, argumentsList ) */             \
  TFC(CallProxy, CallTrampoline, 1)                                            \
  ASM(CallVarargs)                                                             \
  TFC(CallWithSpread, CallWithSpread, 1)                                       \
  TFC(CallWithArrayLike, CallWithArrayLike, 1)                                 \
  ASM(CallForwardVarargs)                                                      \
  ASM(CallFunctionForwardVarargs)                                              \
                                                                               \
  /* Construct */                                                              \
  /* ES6 section 9.2.2 [[Construct]] ( argumentsList, newTarget) */            \
  ASM(ConstructFunction)                                                       \
  /* ES6 section 9.4.1.2 [[Construct]] (argumentsList, newTarget) */           \
  ASM(ConstructBoundFunction)                                                  \
  ASM(ConstructedNonConstructable)                                             \
  /* ES6 section 7.3.13 Construct (F, [argumentsList], [newTarget]) */         \
  ASM(Construct)                                                               \
  ASM(ConstructVarargs)                                                        \
  TFC(ConstructWithSpread, ConstructWithSpread, 1)                             \
  TFC(ConstructWithArrayLike, ConstructWithArrayLike, 1)                       \
  ASM(ConstructForwardVarargs)                                                 \
  ASM(ConstructFunctionForwardVarargs)                                         \
  ASM(JSConstructStubGenericRestrictedReturn)                                  \
  ASM(JSConstructStubGenericUnrestrictedReturn)                                \
  ASM(JSBuiltinsConstructStub)                                                 \
  TFC(FastNewObject, FastNewObject, 1)                                         \
  TFS(FastNewClosure, kSharedFunctionInfo, kFeedbackCell)                      \
  TFC(FastNewFunctionContextEval, FastNewFunctionContext, 1)                   \
  TFC(FastNewFunctionContextFunction, FastNewFunctionContext, 1)               \
  TFS(CreateRegExpLiteral, kFeedbackVector, kSlot, kPattern, kFlags)           \
  TFS(CreateEmptyArrayLiteral, kFeedbackVector, kSlot)                         \
  TFS(CreateShallowArrayLiteral, kFeedbackVector, kSlot, kConstantElements)    \
  TFS(CreateShallowObjectLiteral, kFeedbackVector, kSlot,                      \
      kBoilerplateDescription, kFlags)                                         \
  /* ES6 section 9.5.14 [[Construct]] ( argumentsList, newTarget) */           \
  TFC(ConstructProxy, ConstructTrampoline, 1)                                  \
                                                                               \
  /* Apply and entries */                                                      \
  ASM(JSEntryTrampoline)                                                       \
  ASM(JSConstructEntryTrampoline)                                              \
  ASM(ResumeGeneratorTrampoline)                                               \
                                                                               \
  /* Stack and interrupt check */                                              \
  ASM(InterruptCheck)                                                          \
  ASM(StackCheck)                                                              \
                                                                               \
  /* String helpers */                                                         \
  TFC(StringCharAt, StringAt, 1)                                               \
  TFC(StringCodePointAtUTF16, StringAt, 1)                                     \
  TFC(StringCodePointAtUTF32, StringAt, 1)                                     \
  TFC(StringEqual, Compare, 1)                                                 \
  TFC(StringGreaterThan, Compare, 1)                                           \
  TFC(StringGreaterThanOrEqual, Compare, 1)                                    \
  TFS(StringIndexOf, kReceiver, kSearchString, kPosition)                      \
  TFC(StringLessThan, Compare, 1)                                              \
  TFC(StringLessThanOrEqual, Compare, 1)                                       \
  TFS(StringRepeat, kString, kCount)                                           \
  TFC(StringSubstring, StringSubstring, 1)                                     \
                                                                               \
  /* OrderedHashTable helpers */                                               \
  TFS(OrderedHashTableHealIndex, kTable, kIndex)                               \
                                                                               \
  /* Interpreter */                                                            \
  ASM(InterpreterEntryTrampoline)                                              \
  ASM(InterpreterPushArgsThenCall)                                             \
  ASM(InterpreterPushUndefinedAndArgsThenCall)                                 \
  ASM(InterpreterPushArgsThenCallWithFinalSpread)                              \
  ASM(InterpreterPushArgsThenConstruct)                                        \
  ASM(InterpreterPushArgsThenConstructArrayFunction)                           \
  ASM(InterpreterPushArgsThenConstructWithFinalSpread)                         \
  ASM(InterpreterEnterBytecodeAdvance)                                         \
  ASM(InterpreterEnterBytecodeDispatch)                                        \
  ASM(InterpreterOnStackReplacement)                                           \
                                                                               \
  /* Code life-cycle */                                                        \
  ASM(CompileLazy)                                                             \
  ASM(CompileLazyDeoptimizedCode)                                              \
  ASM(DeserializeLazy)                                                         \
  ASM(InstantiateAsmJs)                                                        \
  ASM(NotifyDeoptimized)                                                       \
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
  ASM(ContinueToCodeStubBuiltin)                                               \
  ASM(ContinueToCodeStubBuiltinWithResult)                                     \
  ASM(ContinueToJavaScriptBuiltin)                                             \
  ASM(ContinueToJavaScriptBuiltinWithResult)                                   \
                                                                               \
  ASM(OnStackReplacement)                                                      \
                                                                               \
  /* API callback handling */                                                  \
  API(HandleApiCall)                                                           \
  API(HandleApiCallAsFunction)                                                 \
  API(HandleApiCallAsConstructor)                                              \
                                                                               \
  /* Adapters for Turbofan into runtime */                                     \
  ASM(AllocateInNewSpace)                                                      \
  ASM(AllocateInOldSpace)                                                      \
                                                                               \
  /* TurboFan support builtins */                                              \
  TFS(CopyFastSmiOrObjectElements, kObject)                                    \
  TFC(GrowFastDoubleElements, GrowArrayElements, 1)                            \
  TFC(GrowFastSmiOrObjectElements, GrowArrayElements, 1)                       \
  TFC(NewArgumentsElements, NewArgumentsElements, 1)                           \
                                                                               \
  /* Debugger */                                                               \
  TFJ(DebugBreakTrampoline, SharedFunctionInfo::kDontAdaptArgumentsSentinel)   \
  ASM(FrameDropperTrampoline)                                                  \
  ASM(HandleDebuggerStatement)                                                 \
                                                                               \
  /* Type conversions */                                                       \
  TFC(ToObject, TypeConversion, 1)                                             \
  TFC(ToBoolean, TypeConversion, 1)                                            \
  TFC(OrdinaryToPrimitive_Number, TypeConversion, 1)                           \
  TFC(OrdinaryToPrimitive_String, TypeConversion, 1)                           \
  TFC(NonPrimitiveToPrimitive_Default, TypeConversion, 1)                      \
  TFC(NonPrimitiveToPrimitive_Number, TypeConversion, 1)                       \
  TFC(NonPrimitiveToPrimitive_String, TypeConversion, 1)                       \
  TFC(StringToNumber, TypeConversion, 1)                                       \
  TFC(ToName, TypeConversion, 1)                                               \
  TFC(NonNumberToNumber, TypeConversion, 1)                                    \
  TFC(NonNumberToNumeric, TypeConversion, 1)                                   \
  TFC(ToNumber, TypeConversion, 1)                                             \
  TFC(ToNumeric, TypeConversion, 1)                                            \
  TFC(NumberToString, TypeConversion, 1)                                       \
  TFC(ToString, TypeConversion, 1)                                             \
  TFC(ToInteger, TypeConversion, 1)                                            \
  TFC(ToInteger_TruncateMinusZero, TypeConversion, 1)                          \
  TFC(ToLength, TypeConversion, 1)                                             \
  TFC(Typeof, Typeof, 1)                                                       \
  TFC(GetSuperConstructor, Typeof, 1)                                          \
                                                                               \
  /* Type conversions continuations */                                         \
  TFC(ToBooleanLazyDeoptContinuation, TypeConversionStackParameter, 1)         \
                                                                               \
  /* Handlers */                                                               \
  TFH(KeyedLoadIC_Megamorphic, LoadWithVector)                                 \
  TFH(KeyedLoadIC_PolymorphicName, LoadWithVector)                             \
  TFH(KeyedLoadIC_Slow, LoadWithVector)                                        \
  TFH(KeyedStoreIC_Megamorphic, StoreWithVector)                               \
  TFH(KeyedStoreIC_Slow, StoreWithVector)                                      \
  TFH(LoadGlobalIC_Slow, LoadWithVector)                                       \
  TFH(LoadField, LoadField)                                                    \
  TFH(LoadIC_FunctionPrototype, LoadWithVector)                                \
  TFH(LoadIC_Slow, LoadWithVector)                                             \
  TFH(LoadIC_StringLength, LoadWithVector)                                     \
  TFH(LoadIC_StringWrapperLength, LoadWithVector)                              \
  TFH(LoadIC_Uninitialized, LoadWithVector)                                    \
  TFH(StoreGlobalIC_Slow, StoreWithVector)                                     \
  TFH(StoreIC_Uninitialized, StoreWithVector)                                  \
  TFH(StoreInArrayLiteralIC_Slow, StoreWithVector)                             \
                                                                               \
  /* Microtask helpers */                                                      \
  TFS(EnqueueMicrotask, kMicrotask)                                            \
  TFC(RunMicrotasks, RunMicrotasks, 1)                                         \
                                                                               \
  /* Object property helpers */                                                \
  TFS(HasProperty, kKey, kObject)                                              \
  TFS(DeleteProperty, kObject, kKey, kLanguageMode)                            \
                                                                               \
  /* Abort */                                                                  \
  ASM(Abort)                                                                   \
  TFC(AbortJS, AbortJS, 1)                                                     \
                                                                               \
  /* Built-in functions for Javascript */                                      \
  /* Special internal builtins */                                              \
  CPP(EmptyFunction)                                                           \
  CPP(Illegal)                                                                 \
  CPP(StrictPoisonPillThrower)                                                 \
  CPP(UnsupportedThrower)                                                      \
  TFJ(ReturnReceiver, 0)                                                       \
                                                                               \
  /* Array */                                                                  \
  ASM(ArrayConstructor)                                                        \
  ASM(InternalArrayConstructor)                                                \
  CPP(ArrayConcat)                                                             \
  /* ES6 #sec-array.isarray */                                                 \
  TFJ(ArrayIsArray, 1, kArg)                                                   \
  /* ES6 #sec-array.from */                                                    \
  TFJ(ArrayFrom, SharedFunctionInfo::kDontAdaptArgumentsSentinel)              \
  /* ES6 #sec-array.of */                                                      \
  TFJ(ArrayOf, SharedFunctionInfo::kDontAdaptArgumentsSentinel)                \
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
  TFJ(ArrayPrototypeShift, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
  /* ES6 #sec-array.prototype.slice */                                         \
  TFJ(ArrayPrototypeSlice, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
  /* ES6 #sec-array.prototype.splice */                                        \
  CPP(ArraySplice)                                                             \
  /* ES6 #sec-array.prototype.unshift */                                       \
  CPP(ArrayUnshift)                                                            \
  /* Support for Array.from and other array-copying idioms */                  \
  TFS(CloneFastJSArray, kSource)                                               \
  TFS(ExtractFastJSArray, kSource, kBegin, kCount)                             \
  /* ES6 #sec-array.prototype.foreach */                                       \
  TFS(ArrayForEachLoopContinuation, kReceiver, kCallbackFn, kThisArg, kArray,  \
      kObject, kInitialK, kLength, kTo)                                        \
  TFJ(ArrayForEachLoopEagerDeoptContinuation, 4, kCallbackFn, kThisArg,        \
      kInitialK, kLength)                                                      \
  TFJ(ArrayForEachLoopLazyDeoptContinuation, 5, kCallbackFn, kThisArg,         \
      kInitialK, kLength, kResult)                                             \
  TFJ(ArrayForEach, SharedFunctionInfo::kDontAdaptArgumentsSentinel)           \
  /* ES6 #sec-array.prototype.every */                                         \
  TFS(ArrayEveryLoopContinuation, kReceiver, kCallbackFn, kThisArg, kArray,    \
      kObject, kInitialK, kLength, kTo)                                        \
  TFJ(ArrayEveryLoopEagerDeoptContinuation, 4, kCallbackFn, kThisArg,          \
      kInitialK, kLength)                                                      \
  TFJ(ArrayEveryLoopLazyDeoptContinuation, 5, kCallbackFn, kThisArg,           \
      kInitialK, kLength, kResult)                                             \
  TFJ(ArrayEvery, SharedFunctionInfo::kDontAdaptArgumentsSentinel)             \
  /* ES6 #sec-array.prototype.some */                                          \
  TFS(ArraySomeLoopContinuation, kReceiver, kCallbackFn, kThisArg, kArray,     \
      kObject, kInitialK, kLength, kTo)                                        \
  TFJ(ArraySomeLoopEagerDeoptContinuation, 4, kCallbackFn, kThisArg,           \
      kInitialK, kLength)                                                      \
  TFJ(ArraySomeLoopLazyDeoptContinuation, 5, kCallbackFn, kThisArg, kInitialK, \
      kLength, kResult)                                                        \
  TFJ(ArraySome, SharedFunctionInfo::kDontAdaptArgumentsSentinel)              \
  /* ES6 #sec-array.prototype.filter */                                        \
  TFS(ArrayFilterLoopContinuation, kReceiver, kCallbackFn, kThisArg, kArray,   \
      kObject, kInitialK, kLength, kTo)                                        \
  TFJ(ArrayFilter, SharedFunctionInfo::kDontAdaptArgumentsSentinel)            \
  TFJ(ArrayFilterLoopEagerDeoptContinuation, 6, kCallbackFn, kThisArg, kArray, \
      kInitialK, kLength, kTo)                                                 \
  TFJ(ArrayFilterLoopLazyDeoptContinuation, 8, kCallbackFn, kThisArg, kArray,  \
      kInitialK, kLength, kValueK, kTo, kResult)                               \
  /* ES6 #sec-array.prototype.foreach */                                       \
  TFS(ArrayMapLoopContinuation, kReceiver, kCallbackFn, kThisArg, kArray,      \
      kObject, kInitialK, kLength, kTo)                                        \
  TFJ(ArrayMapLoopEagerDeoptContinuation, 5, kCallbackFn, kThisArg, kArray,    \
      kInitialK, kLength)                                                      \
  TFJ(ArrayMapLoopLazyDeoptContinuation, 6, kCallbackFn, kThisArg, kArray,     \
      kInitialK, kLength, kResult)                                             \
  TFJ(ArrayMap, SharedFunctionInfo::kDontAdaptArgumentsSentinel)               \
  /* ES6 #sec-array.prototype.reduce */                                        \
  TFS(ArrayReduceLoopContinuation, kReceiver, kCallbackFn, kThisArg,           \
      kAccumulator, kObject, kInitialK, kLength, kTo)                          \
  TFJ(ArrayReducePreLoopEagerDeoptContinuation, 2, kCallbackFn, kLength)       \
  TFJ(ArrayReduceLoopEagerDeoptContinuation, 4, kCallbackFn, kInitialK,        \
      kLength, kAccumulator)                                                   \
  TFJ(ArrayReduceLoopLazyDeoptContinuation, 4, kCallbackFn, kInitialK,         \
      kLength, kResult)                                                        \
  TFJ(ArrayReduce, SharedFunctionInfo::kDontAdaptArgumentsSentinel)            \
  /* ES6 #sec-array.prototype.reduceRight */                                   \
  TFS(ArrayReduceRightLoopContinuation, kReceiver, kCallbackFn, kThisArg,      \
      kAccumulator, kObject, kInitialK, kLength, kTo)                          \
  TFJ(ArrayReduceRightPreLoopEagerDeoptContinuation, 2, kCallbackFn, kLength)  \
  TFJ(ArrayReduceRightLoopEagerDeoptContinuation, 4, kCallbackFn, kInitialK,   \
      kLength, kAccumulator)                                                   \
  TFJ(ArrayReduceRightLoopLazyDeoptContinuation, 4, kCallbackFn, kInitialK,    \
      kLength, kResult)                                                        \
  TFJ(ArrayReduceRight, SharedFunctionInfo::kDontAdaptArgumentsSentinel)       \
  /* ES6 #sec-array.prototype.entries */                                       \
  TFJ(ArrayPrototypeEntries, 0)                                                \
  /* ES6 #sec-array.prototype.find */                                          \
  TFS(ArrayFindLoopContinuation, kReceiver, kCallbackFn, kThisArg, kArray,     \
      kObject, kInitialK, kLength, kTo)                                        \
  TFJ(ArrayFindLoopEagerDeoptContinuation, 4, kCallbackFn, kThisArg,           \
      kInitialK, kLength)                                                      \
  TFJ(ArrayFindLoopLazyDeoptContinuation, 5, kCallbackFn, kThisArg, kInitialK, \
      kLength, kResult)                                                        \
  TFJ(ArrayFindLoopAfterCallbackLazyDeoptContinuation, 6, kCallbackFn,         \
      kThisArg, kInitialK, kLength, kFoundValue, kIsFound)                     \
  TFJ(ArrayPrototypeFind, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
  /* ES6 #sec-array.prototype.findIndex */                                     \
  TFS(ArrayFindIndexLoopContinuation, kReceiver, kCallbackFn, kThisArg,        \
      kArray, kObject, kInitialK, kLength, kTo)                                \
  TFJ(ArrayFindIndexLoopEagerDeoptContinuation, 4, kCallbackFn, kThisArg,      \
      kInitialK, kLength)                                                      \
  TFJ(ArrayFindIndexLoopLazyDeoptContinuation, 5, kCallbackFn, kThisArg,       \
      kInitialK, kLength, kResult)                                             \
  TFJ(ArrayFindIndexLoopAfterCallbackLazyDeoptContinuation, 6, kCallbackFn,    \
      kThisArg, kInitialK, kLength, kFoundValue, kIsFound)                     \
  TFJ(ArrayPrototypeFindIndex,                                                 \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-array.prototype.keys */                                          \
  TFJ(ArrayPrototypeKeys, 0)                                                   \
  /* ES6 #sec-array.prototype.values */                                        \
  TFJ(ArrayPrototypeValues, 0)                                                 \
  /* ES6 #sec-%arrayiteratorprototype%.next */                                 \
  TFJ(ArrayIteratorPrototypeNext, 0)                                           \
  /* https://tc39.github.io/proposal-flatMap/#sec-FlattenIntoArray */          \
  TFS(FlattenIntoArray, kTarget, kSource, kSourceLength, kStart, kDepth)       \
  TFS(FlatMapIntoArray, kTarget, kSource, kSourceLength, kStart, kDepth,       \
      kMapperFunction, kThisArg)                                               \
  /* https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatten */   \
  TFJ(ArrayPrototypeFlatten, SharedFunctionInfo::kDontAdaptArgumentsSentinel)  \
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
  TFC(AsyncFunctionAwaitFulfill, PromiseReactionHandler, 1)                    \
  TFC(AsyncFunctionAwaitReject, PromiseReactionHandler, 1)                     \
  TFS(AsyncFunctionAwaitCaught, kGenerator, kValue, kOuterPromise)             \
  TFS(AsyncFunctionAwaitUncaught, kGenerator, kValue, kOuterPromise)           \
  TFJ(AsyncFunctionPromiseCreate, 0)                                           \
  TFJ(AsyncFunctionPromiseRelease, 1, kPromise)                                \
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
  /* ES #sec-boolean-constructor */                                            \
  CPP(BooleanConstructor)                                                      \
  /* ES6 #sec-boolean.prototype.tostring */                                    \
  TFJ(BooleanPrototypeToString, 0)                                             \
  /* ES6 #sec-boolean.prototype.valueof */                                     \
  TFJ(BooleanPrototypeValueOf, 0)                                              \
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
  CPP(CallSitePrototypeGetScriptNameOrSourceURL)                               \
  CPP(CallSitePrototypeGetThis)                                                \
  CPP(CallSitePrototypeGetTypeName)                                            \
  CPP(CallSitePrototypeIsConstructor)                                          \
  CPP(CallSitePrototypeIsEval)                                                 \
  CPP(CallSitePrototypeIsNative)                                               \
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
  CPP(ConsoleAssert)                                                           \
  TFJ(FastConsoleAssert, SharedFunctionInfo::kDontAdaptArgumentsSentinel)      \
  CPP(ConsoleMarkTimeline)                                                     \
  CPP(ConsoleProfile)                                                          \
  CPP(ConsoleProfileEnd)                                                       \
  CPP(ConsoleTimeline)                                                         \
  CPP(ConsoleTimelineEnd)                                                      \
  CPP(ConsoleTime)                                                             \
  CPP(ConsoleTimeEnd)                                                          \
  CPP(ConsoleTimeStamp)                                                        \
  CPP(ConsoleContext)                                                          \
                                                                               \
  /* DataView */                                                               \
  /* ES #sec-dataview-constructor */                                           \
  CPP(DataViewConstructor)                                                     \
  CPP(DataViewPrototypeGetBuffer)                                              \
  CPP(DataViewPrototypeGetByteLength)                                          \
  CPP(DataViewPrototypeGetByteOffset)                                          \
  CPP(DataViewPrototypeGetInt8)                                                \
  CPP(DataViewPrototypeSetInt8)                                                \
  CPP(DataViewPrototypeGetUint8)                                               \
  CPP(DataViewPrototypeSetUint8)                                               \
  CPP(DataViewPrototypeGetInt16)                                               \
  CPP(DataViewPrototypeSetInt16)                                               \
  CPP(DataViewPrototypeGetUint16)                                              \
  CPP(DataViewPrototypeSetUint16)                                              \
  CPP(DataViewPrototypeGetInt32)                                               \
  CPP(DataViewPrototypeSetInt32)                                               \
  CPP(DataViewPrototypeGetUint32)                                              \
  CPP(DataViewPrototypeSetUint32)                                              \
  CPP(DataViewPrototypeGetFloat32)                                             \
  CPP(DataViewPrototypeSetFloat32)                                             \
  CPP(DataViewPrototypeGetFloat64)                                             \
  CPP(DataViewPrototypeSetFloat64)                                             \
  CPP(DataViewPrototypeGetBigInt64)                                            \
  CPP(DataViewPrototypeSetBigInt64)                                            \
  CPP(DataViewPrototypeGetBigUint64)                                           \
  CPP(DataViewPrototypeSetBigUint64)                                           \
                                                                               \
  /* Date */                                                                   \
  /* ES #sec-date-constructor */                                               \
  CPP(DateConstructor)                                                         \
  /* ES6 #sec-date.prototype.getdate */                                        \
  TFJ(DatePrototypeGetDate, 0)                                                 \
  /* ES6 #sec-date.prototype.getday */                                         \
  TFJ(DatePrototypeGetDay, 0)                                                  \
  /* ES6 #sec-date.prototype.getfullyear */                                    \
  TFJ(DatePrototypeGetFullYear, 0)                                             \
  /* ES6 #sec-date.prototype.gethours */                                       \
  TFJ(DatePrototypeGetHours, 0)                                                \
  /* ES6 #sec-date.prototype.getmilliseconds */                                \
  TFJ(DatePrototypeGetMilliseconds, 0)                                         \
  /* ES6 #sec-date.prototype.getminutes */                                     \
  TFJ(DatePrototypeGetMinutes, 0)                                              \
  /* ES6 #sec-date.prototype.getmonth */                                       \
  TFJ(DatePrototypeGetMonth, 0)                                                \
  /* ES6 #sec-date.prototype.getseconds */                                     \
  TFJ(DatePrototypeGetSeconds, 0)                                              \
  /* ES6 #sec-date.prototype.gettime */                                        \
  TFJ(DatePrototypeGetTime, 0)                                                 \
  /* ES6 #sec-date.prototype.gettimezoneoffset */                              \
  TFJ(DatePrototypeGetTimezoneOffset, 0)                                       \
  /* ES6 #sec-date.prototype.getutcdate */                                     \
  TFJ(DatePrototypeGetUTCDate, 0)                                              \
  /* ES6 #sec-date.prototype.getutcday */                                      \
  TFJ(DatePrototypeGetUTCDay, 0)                                               \
  /* ES6 #sec-date.prototype.getutcfullyear */                                 \
  TFJ(DatePrototypeGetUTCFullYear, 0)                                          \
  /* ES6 #sec-date.prototype.getutchours */                                    \
  TFJ(DatePrototypeGetUTCHours, 0)                                             \
  /* ES6 #sec-date.prototype.getutcmilliseconds */                             \
  TFJ(DatePrototypeGetUTCMilliseconds, 0)                                      \
  /* ES6 #sec-date.prototype.getutcminutes */                                  \
  TFJ(DatePrototypeGetUTCMinutes, 0)                                           \
  /* ES6 #sec-date.prototype.getutcmonth */                                    \
  TFJ(DatePrototypeGetUTCMonth, 0)                                             \
  /* ES6 #sec-date.prototype.getutcseconds */                                  \
  TFJ(DatePrototypeGetUTCSeconds, 0)                                           \
  /* ES6 #sec-date.prototype.valueof */                                        \
  TFJ(DatePrototypeValueOf, 0)                                                 \
  /* ES6 #sec-date.prototype-@@toprimitive */                                  \
  TFJ(DatePrototypeToPrimitive, 1, kHint)                                      \
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
  /* Function */                                                               \
  CPP(FunctionConstructor)                                                     \
  ASM(FunctionPrototypeApply)                                                  \
  CPP(FunctionPrototypeBind)                                                   \
  /* ES6 #sec-function.prototype.bind */                                       \
  TFJ(FastFunctionPrototypeBind,                                               \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  ASM(FunctionPrototypeCall)                                                   \
  /* ES6 #sec-function.prototype-@@hasinstance */                              \
  TFJ(FunctionPrototypeHasInstance, 1, kV)                                     \
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
  /* Global object */                                                          \
  CPP(GlobalDecodeURI)                                                         \
  CPP(GlobalDecodeURIComponent)                                                \
  CPP(GlobalEncodeURI)                                                         \
  CPP(GlobalEncodeURIComponent)                                                \
  CPP(GlobalEscape)                                                            \
  CPP(GlobalUnescape)                                                          \
  CPP(GlobalEval)                                                              \
  /* ES6 #sec-isfinite-number */                                               \
  TFJ(GlobalIsFinite, 1, kNumber)                                              \
  /* ES6 #sec-isnan-number */                                                  \
  TFJ(GlobalIsNaN, 1, kNumber)                                                 \
                                                                               \
  /* JSON */                                                                   \
  CPP(JsonParse)                                                               \
  CPP(JsonStringify)                                                           \
                                                                               \
  /* ICs */                                                                    \
  TFH(LoadIC, LoadWithVector)                                                  \
  TFH(LoadIC_Noninlined, LoadWithVector)                                       \
  TFH(LoadICTrampoline, Load)                                                  \
  TFH(KeyedLoadIC, LoadWithVector)                                             \
  TFH(KeyedLoadICTrampoline, Load)                                             \
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
                                                                               \
  /* Map */                                                                    \
  TFS(FindOrderedHashMapEntry, kTable, kKey)                                   \
  TFJ(MapConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)         \
  TFJ(MapPrototypeSet, 2, kKey, kValue)                                        \
  TFJ(MapPrototypeDelete, 1, kKey)                                             \
  TFJ(MapPrototypeGet, 1, kKey)                                                \
  TFJ(MapPrototypeHas, 1, kKey)                                                \
  CPP(MapPrototypeClear)                                                       \
  /* ES #sec-map.prototype.entries */                                          \
  TFJ(MapPrototypeEntries, 0)                                                  \
  /* ES #sec-get-map.prototype.size */                                         \
  TFJ(MapPrototypeGetSize, 0)                                                  \
  /* ES #sec-map.prototype.forEach */                                          \
  TFJ(MapPrototypeForEach, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
  /* ES #sec-map.prototype.keys */                                             \
  TFJ(MapPrototypeKeys, 0)                                                     \
  /* ES #sec-map.prototype.values */                                           \
  TFJ(MapPrototypeValues, 0)                                                   \
  /* ES #sec-%mapiteratorprototype%.next */                                    \
  TFJ(MapIteratorPrototypeNext, 0)                                             \
                                                                               \
  /* Math */                                                                   \
  /* ES6 #sec-math.abs */                                                      \
  TFJ(MathAbs, 1, kX)                                                          \
  /* ES6 #sec-math.acos */                                                     \
  TFJ(MathAcos, 1, kX)                                                         \
  /* ES6 #sec-math.acosh */                                                    \
  TFJ(MathAcosh, 1, kX)                                                        \
  /* ES6 #sec-math.asin */                                                     \
  TFJ(MathAsin, 1, kX)                                                         \
  /* ES6 #sec-math.asinh */                                                    \
  TFJ(MathAsinh, 1, kX)                                                        \
  /* ES6 #sec-math.atan */                                                     \
  TFJ(MathAtan, 1, kX)                                                         \
  /* ES6 #sec-math.atanh */                                                    \
  TFJ(MathAtanh, 1, kX)                                                        \
  /* ES6 #sec-math.atan2 */                                                    \
  TFJ(MathAtan2, 2, kY, kX)                                                    \
  /* ES6 #sec-math.cbrt */                                                     \
  TFJ(MathCbrt, 1, kX)                                                         \
  /* ES6 #sec-math.ceil */                                                     \
  TFJ(MathCeil, 1, kX)                                                         \
  /* ES6 #sec-math.clz32 */                                                    \
  TFJ(MathClz32, 1, kX)                                                        \
  /* ES6 #sec-math.cos */                                                      \
  TFJ(MathCos, 1, kX)                                                          \
  /* ES6 #sec-math.cosh */                                                     \
  TFJ(MathCosh, 1, kX)                                                         \
  /* ES6 #sec-math.exp */                                                      \
  TFJ(MathExp, 1, kX)                                                          \
  /* ES6 #sec-math.expm1 */                                                    \
  TFJ(MathExpm1, 1, kX)                                                        \
  /* ES6 #sec-math.floor */                                                    \
  TFJ(MathFloor, 1, kX)                                                        \
  /* ES6 #sec-math.fround */                                                   \
  TFJ(MathFround, 1, kX)                                                       \
  /* ES6 #sec-math.hypot */                                                    \
  CPP(MathHypot)                                                               \
  /* ES6 #sec-math.imul */                                                     \
  TFJ(MathImul, 2, kX, kY)                                                     \
  /* ES6 #sec-math.log */                                                      \
  TFJ(MathLog, 1, kX)                                                          \
  /* ES6 #sec-math.log1p */                                                    \
  TFJ(MathLog1p, 1, kX)                                                        \
  /* ES6 #sec-math.log10 */                                                    \
  TFJ(MathLog10, 1, kX)                                                        \
  /* ES6 #sec-math.log2 */                                                     \
  TFJ(MathLog2, 1, kX)                                                         \
  /* ES6 #sec-math.max */                                                      \
  TFJ(MathMax, SharedFunctionInfo::kDontAdaptArgumentsSentinel)                \
  /* ES6 #sec-math.min */                                                      \
  TFJ(MathMin, SharedFunctionInfo::kDontAdaptArgumentsSentinel)                \
  /* ES6 #sec-math.pow */                                                      \
  TFJ(MathPow, 2, kBase, kExponent)                                            \
  /* ES6 #sec-math.random */                                                   \
  TFJ(MathRandom, 0)                                                           \
  /* ES6 #sec-math.round */                                                    \
  TFJ(MathRound, 1, kX)                                                        \
  /* ES6 #sec-math.sign */                                                     \
  TFJ(MathSign, 1, kX)                                                         \
  /* ES6 #sec-math.sin */                                                      \
  TFJ(MathSin, 1, kX)                                                          \
  /* ES6 #sec-math.sinh */                                                     \
  TFJ(MathSinh, 1, kX)                                                         \
  /* ES6 #sec-math.sqrt */                                                     \
  TFJ(MathTan, 1, kX)                                                          \
  /* ES6 #sec-math.tan */                                                      \
  TFJ(MathTanh, 1, kX)                                                         \
  /* ES6 #sec-math.tanh */                                                     \
  TFJ(MathSqrt, 1, kX)                                                         \
  /* ES6 #sec-math.trunc */                                                    \
  TFJ(MathTrunc, 1, kX)                                                        \
                                                                               \
  /* Number */                                                                 \
  TFC(AllocateHeapNumber, AllocateHeapNumber, 1)                               \
  /* ES #sec-number-constructor */                                             \
  TFJ(NumberConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)      \
  /* ES6 #sec-number.isfinite */                                               \
  TFJ(NumberIsFinite, 1, kNumber)                                              \
  /* ES6 #sec-number.isinteger */                                              \
  TFJ(NumberIsInteger, 1, kNumber)                                             \
  /* ES6 #sec-number.isnan */                                                  \
  TFJ(NumberIsNaN, 1, kNumber)                                                 \
  /* ES6 #sec-number.issafeinteger */                                          \
  TFJ(NumberIsSafeInteger, 1, kNumber)                                         \
  /* ES6 #sec-number.parsefloat */                                             \
  TFJ(NumberParseFloat, 1, kString)                                            \
  /* ES6 #sec-number.parseint */                                               \
  TFJ(NumberParseInt, 2, kString, kRadix)                                      \
  CPP(NumberPrototypeToExponential)                                            \
  CPP(NumberPrototypeToFixed)                                                  \
  CPP(NumberPrototypeToLocaleString)                                           \
  CPP(NumberPrototypeToPrecision)                                              \
  CPP(NumberPrototypeToString)                                                 \
  /* ES6 #sec-number.prototype.valueof */                                      \
  TFJ(NumberPrototypeValueOf, 0)                                               \
  TFC(Add, BinaryOp, 1)                                                        \
  TFC(Subtract, BinaryOp, 1)                                                   \
  TFC(Multiply, BinaryOp, 1)                                                   \
  TFC(Divide, BinaryOp, 1)                                                     \
  TFC(Modulus, BinaryOp, 1)                                                    \
  TFC(Exponentiate, BinaryOp, 1)                                               \
  TFC(BitwiseAnd, BinaryOp, 1)                                                 \
  TFC(BitwiseOr, BinaryOp, 1)                                                  \
  TFC(BitwiseXor, BinaryOp, 1)                                                 \
  TFC(ShiftLeft, BinaryOp, 1)                                                  \
  TFC(ShiftRight, BinaryOp, 1)                                                 \
  TFC(ShiftRightLogical, BinaryOp, 1)                                          \
  TFC(LessThan, Compare, 1)                                                    \
  TFC(LessThanOrEqual, Compare, 1)                                             \
  TFC(GreaterThan, Compare, 1)                                                 \
  TFC(GreaterThanOrEqual, Compare, 1)                                          \
  TFC(Equal, Compare, 1)                                                       \
  TFC(SameValue, Compare, 1)                                                   \
  TFC(StrictEqual, Compare, 1)                                                 \
  TFS(BitwiseNot, kValue)                                                      \
  TFS(Decrement, kValue)                                                       \
  TFS(Increment, kValue)                                                       \
  TFS(Negate, kValue)                                                          \
                                                                               \
  /* Object */                                                                 \
  /* ES #sec-object-constructor */                                             \
  TFJ(ObjectConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)      \
  CPP(ObjectAssign)                                                            \
  /* ES #sec-object.create */                                                  \
  TFJ(ObjectCreate, SharedFunctionInfo::kDontAdaptArgumentsSentinel)           \
  CPP(ObjectDefineGetter)                                                      \
  CPP(ObjectDefineProperties)                                                  \
  CPP(ObjectDefineProperty)                                                    \
  CPP(ObjectDefineSetter)                                                      \
  TFJ(ObjectEntries, 1, kObject)                                               \
  CPP(ObjectFreeze)                                                            \
  TFJ(ObjectGetOwnPropertyDescriptor,                                          \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  CPP(ObjectGetOwnPropertyDescriptors)                                         \
  CPP(ObjectGetOwnPropertyNames)                                               \
  CPP(ObjectGetOwnPropertySymbols)                                             \
  CPP(ObjectGetPrototypeOf)                                                    \
  CPP(ObjectSetPrototypeOf)                                                    \
  TFJ(ObjectIs, 2, kLeft, kRight)                                              \
  CPP(ObjectIsExtensible)                                                      \
  CPP(ObjectIsFrozen)                                                          \
  CPP(ObjectIsSealed)                                                          \
  TFJ(ObjectKeys, 1, kObject)                                                  \
  CPP(ObjectLookupGetter)                                                      \
  CPP(ObjectLookupSetter)                                                      \
  CPP(ObjectPreventExtensions)                                                 \
  /* ES6 #sec-object.prototype.tostring */                                     \
  TFJ(ObjectPrototypeToString, 0)                                              \
  /* ES6 #sec-object.prototype.valueof */                                      \
  TFJ(ObjectPrototypeValueOf, 0)                                               \
  /* ES6 #sec-object.prototype.hasownproperty */                               \
  TFJ(ObjectPrototypeHasOwnProperty, 1, kKey)                                  \
  TFJ(ObjectPrototypeIsPrototypeOf, 1, kValue)                                 \
  CPP(ObjectPrototypePropertyIsEnumerable)                                     \
  CPP(ObjectPrototypeGetProto)                                                 \
  CPP(ObjectPrototypeSetProto)                                                 \
  /* ES #sec-object.prototype.tolocalestring */                                \
  TFJ(ObjectPrototypeToLocaleString, 0)                                        \
  CPP(ObjectSeal)                                                              \
  TFJ(ObjectValues, 1, kObject)                                                \
                                                                               \
  /* instanceof */                                                             \
  TFC(OrdinaryHasInstance, Compare, 1)                                         \
  TFC(InstanceOf, Compare, 1)                                                  \
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
  TFJ(PromiseCapabilityDefaultReject, 1, kReason)                              \
  /* ES #sec-promise-resolve-functions */                                      \
  TFJ(PromiseCapabilityDefaultResolve, 1, kResolution)                         \
  /* ES6 #sec-getcapabilitiesexecutor-functions */                             \
  TFJ(PromiseGetCapabilitiesExecutor, 2, kResolve, kReject)                    \
  /* ES6 #sec-newpromisecapability */                                          \
  TFS(NewPromiseCapability, kConstructor, kDebugEvent)                         \
  TFJ(PromiseConstructorLazyDeoptContinuation, 4, kPromise, kReject,           \
      kException, kResult)                                                     \
  /* ES6 #sec-promise-executor */                                              \
  TFJ(PromiseConstructor, 1, kExecutor)                                        \
  CPP(IsPromise)                                                               \
  /* ES #sec-promise.prototype.then */                                         \
  TFJ(PromisePrototypeThen, 2, kOnFulfilled, kOnRejected)                      \
  /* ES #sec-performpromisethen */                                             \
  TFS(PerformPromiseThen, kPromise, kOnFulfilled, kOnRejected, kResultPromise) \
  /* ES #sec-promise.prototype.catch */                                        \
  TFJ(PromisePrototypeCatch, 1, kOnRejected)                                   \
  /* ES #sec-promisereactionjob */                                             \
  TFS(PromiseRejectReactionJob, kReason, kHandler, kPayload)                   \
  TFS(PromiseFulfillReactionJob, kValue, kHandler, kPayload)                   \
  /* ES #sec-promiseresolvethenablejob */                                      \
  TFS(PromiseResolveThenableJob, kPromiseToResolve, kThenable, kThen)          \
  /* ES #sec-promise.resolve */                                                \
  TFJ(PromiseResolveTrampoline, 1, kValue)                                     \
  /* ES #sec-promise-resolve */                                                \
  TFS(PromiseResolve, kConstructor, kValue)                                    \
  /* ES #sec-promise.reject */                                                 \
  TFJ(PromiseReject, 1, kReason)                                               \
  TFJ(PromisePrototypeFinally, 1, kOnFinally)                                  \
  TFJ(PromiseThenFinally, 1, kValue)                                           \
  TFJ(PromiseCatchFinally, 1, kReason)                                         \
  TFJ(PromiseValueThunkFinally, 0)                                             \
  TFJ(PromiseThrowerFinally, 0)                                                \
  /* ES #sec-promise.all */                                                    \
  TFJ(PromiseAll, 1, kIterable)                                                \
  TFJ(PromiseAllResolveElementClosure, 1, kValue)                              \
  /* ES #sec-promise.race */                                                   \
  TFJ(PromiseRace, 1, kIterable)                                               \
  /* V8 Extras: v8.createPromise(parent) */                                    \
  TFJ(PromiseInternalConstructor, 1, kParent)                                  \
  /* V8 Extras: v8.rejectPromise(promise, reason) */                           \
  TFJ(PromiseInternalReject, 2, kPromise, kReason)                             \
  /* V8 Extras: v8.resolvePromise(promise, resolution) */                      \
  TFJ(PromiseInternalResolve, 2, kPromise, kResolution)                        \
                                                                               \
  /* Proxy */                                                                  \
  TFJ(ProxyConstructor, 2, kTarget, kHandler)                                  \
  TFJ(ProxyRevocable, 2, kTarget, kHandler)                                    \
  TFJ(ProxyRevoke, 0)                                                          \
  TFS(ProxyGetProperty, kProxy, kName, kReceiverValue)                         \
  TFS(ProxyHasProperty, kProxy, kName)                                         \
  TFS(ProxySetProperty, kProxy, kName, kValue, kReceiverValue, kLanguageMode)  \
                                                                               \
  /* Reflect */                                                                \
  ASM(ReflectApply)                                                            \
  ASM(ReflectConstruct)                                                        \
  CPP(ReflectDefineProperty)                                                   \
  CPP(ReflectDeleteProperty)                                                   \
  CPP(ReflectGet)                                                              \
  CPP(ReflectGetOwnPropertyDescriptor)                                         \
  CPP(ReflectGetPrototypeOf)                                                   \
  TFJ(ReflectHas, 2, kTarget, kKey)                                            \
  CPP(ReflectIsExtensible)                                                     \
  CPP(ReflectOwnKeys)                                                          \
  CPP(ReflectPreventExtensions)                                                \
  CPP(ReflectSet)                                                              \
  CPP(ReflectSetPrototypeOf)                                                   \
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
  TFJ(RegExpConstructor, 2, kPattern, kFlags)                                  \
  TFJ(RegExpInternalMatch, 2, kRegExp, kString)                                \
  CPP(RegExpInputGetter)                                                       \
  CPP(RegExpInputSetter)                                                       \
  CPP(RegExpLastMatchGetter)                                                   \
  CPP(RegExpLastParenGetter)                                                   \
  CPP(RegExpLeftContextGetter)                                                 \
  /* ES #sec-regexp.prototype.compile */                                       \
  TFJ(RegExpPrototypeCompile, 2, kPattern, kFlags)                             \
  /* ES #sec-regexp.prototype.exec */                                          \
  TFJ(RegExpPrototypeExec, 1, kString)                                         \
  /* ES #sec-get-regexp.prototype.dotAll */                                    \
  TFJ(RegExpPrototypeDotAllGetter, 0)                                          \
  /* ES #sec-get-regexp.prototype.flags */                                     \
  TFJ(RegExpPrototypeFlagsGetter, 0)                                           \
  /* ES #sec-get-regexp.prototype.global */                                    \
  TFJ(RegExpPrototypeGlobalGetter, 0)                                          \
  /* ES #sec-get-regexp.prototype.ignorecase */                                \
  TFJ(RegExpPrototypeIgnoreCaseGetter, 0)                                      \
  /* ES #sec-regexp.prototype-@@match */                                       \
  TFJ(RegExpPrototypeMatch, 1, kString)                                        \
  /* https://tc39.github.io/proposal-string-matchall/ */                       \
  TFJ(RegExpPrototypeMatchAll, 1, kString)                                     \
  /* ES #sec-get-regexp.prototype.multiline */                                 \
  TFJ(RegExpPrototypeMultilineGetter, 0)                                       \
  /* ES #sec-regexp.prototype-@@search */                                      \
  TFJ(RegExpPrototypeSearch, 1, kString)                                       \
  /* ES #sec-get-regexp.prototype.source */                                    \
  TFJ(RegExpPrototypeSourceGetter, 0)                                          \
  /* ES #sec-get-regexp.prototype.sticky */                                    \
  TFJ(RegExpPrototypeStickyGetter, 0)                                          \
  /* ES #sec-regexp.prototype.test */                                          \
  TFJ(RegExpPrototypeTest, 1, kString)                                         \
  CPP(RegExpPrototypeToString)                                                 \
  /* ES #sec-get-regexp.prototype.unicode */                                   \
  TFJ(RegExpPrototypeUnicodeGetter, 0)                                         \
  CPP(RegExpRightContextGetter)                                                \
                                                                               \
  /* ES #sec-regexp.prototype-@@replace */                                     \
  TFJ(RegExpPrototypeReplace, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  /* ES #sec-regexp.prototype-@@split */                                       \
  TFJ(RegExpPrototypeSplit, SharedFunctionInfo::kDontAdaptArgumentsSentinel)   \
  /* RegExp helpers */                                                         \
  TFS(RegExpExecAtom, kRegExp, kString, kLastIndex, kMatchInfo)                \
  TFS(RegExpMatchFast, kReceiver, kPattern)                                    \
  TFS(RegExpPrototypeExecSlow, kReceiver, kString)                             \
  TFS(RegExpReplace, kRegExp, kString, kReplaceValue)                          \
  TFS(RegExpSearchFast, kReceiver, kPattern)                                   \
  TFS(RegExpSplit, kRegExp, kString, kLimit)                                   \
                                                                               \
  /* RegExp String Iterator */                                                 \
  /* https://tc39.github.io/proposal-string-matchall/ */                       \
  TFJ(RegExpStringIteratorPrototypeNext, 0)                                    \
                                                                               \
  /* Set */                                                                    \
  TFJ(SetConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)         \
  TFJ(SetPrototypeHas, 1, kKey)                                                \
  TFJ(SetPrototypeAdd, 1, kKey)                                                \
  TFJ(SetPrototypeDelete, 1, kKey)                                             \
  CPP(SetPrototypeClear)                                                       \
  /* ES #sec-set.prototype.entries */                                          \
  TFJ(SetPrototypeEntries, 0)                                                  \
  /* ES #sec-get-set.prototype.size */                                         \
  TFJ(SetPrototypeGetSize, 0)                                                  \
  /* ES #sec-set.prototype.foreach */                                          \
  TFJ(SetPrototypeForEach, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
  /* ES #sec-set.prototype.values */                                           \
  TFJ(SetPrototypeValues, 0)                                                   \
  /* ES #sec-%setiteratorprototype%.next */                                    \
  TFJ(SetIteratorPrototypeNext, 0)                                             \
                                                                               \
  /* SharedArrayBuffer */                                                      \
  CPP(SharedArrayBufferPrototypeGetByteLength)                                 \
  CPP(SharedArrayBufferPrototypeSlice)                                         \
  TFJ(AtomicsLoad, 2, kArray, kIndex)                                          \
  TFJ(AtomicsStore, 3, kArray, kIndex, kValue)                                 \
  TFJ(AtomicsExchange, 3, kArray, kIndex, kValue)                              \
  TFJ(AtomicsCompareExchange, 4, kArray, kIndex, kOldValue, kNewValue)         \
  TFJ(AtomicsAdd, 3, kArray, kIndex, kValue)                                   \
  TFJ(AtomicsSub, 3, kArray, kIndex, kValue)                                   \
  TFJ(AtomicsAnd, 3, kArray, kIndex, kValue)                                   \
  TFJ(AtomicsOr, 3, kArray, kIndex, kValue)                                    \
  TFJ(AtomicsXor, 3, kArray, kIndex, kValue)                                   \
  CPP(AtomicsIsLockFree)                                                       \
  CPP(AtomicsWait)                                                             \
  CPP(AtomicsWake)                                                             \
                                                                               \
  /* String */                                                                 \
  /* ES #sec-string-constructor */                                             \
  TFJ(StringConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)      \
  /* ES #sec-string.fromcodepoint */                                           \
  CPP(StringFromCodePoint)                                                     \
  /* ES6 #sec-string.fromcharcode */                                           \
  TFJ(StringFromCharCode, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
  /* ES6 #sec-string.prototype.anchor */                                       \
  TFJ(StringPrototypeAnchor, 1, kValue)                                        \
  /* ES6 #sec-string.prototype.big */                                          \
  TFJ(StringPrototypeBig, 0)                                                   \
  /* ES6 #sec-string.prototype.blink */                                        \
  TFJ(StringPrototypeBlink, 0)                                                 \
  /* ES6 #sec-string.prototype.bold */                                         \
  TFJ(StringPrototypeBold, 0)                                                  \
  /* ES6 #sec-string.prototype.charat */                                       \
  TFJ(StringPrototypeCharAt, 1, kPosition)                                     \
  /* ES6 #sec-string.prototype.charcodeat */                                   \
  TFJ(StringPrototypeCharCodeAt, 1, kPosition)                                 \
  /* ES6 #sec-string.prototype.codepointat */                                  \
  TFJ(StringPrototypeCodePointAt, 1, kPosition)                                \
  /* ES6 #sec-string.prototype.concat */                                       \
  TFJ(StringPrototypeConcat, SharedFunctionInfo::kDontAdaptArgumentsSentinel)  \
  /* ES6 #sec-string.prototype.endswith */                                     \
  CPP(StringPrototypeEndsWith)                                                 \
  /* ES6 #sec-string.prototype.fontcolor */                                    \
  TFJ(StringPrototypeFontcolor, 1, kValue)                                     \
  /* ES6 #sec-string.prototype.fontsize */                                     \
  TFJ(StringPrototypeFontsize, 1, kValue)                                      \
  /* ES6 #sec-string.prototype.fixed */                                        \
  TFJ(StringPrototypeFixed, 0)                                                 \
  /* ES6 #sec-string.prototype.includes */                                     \
  TFJ(StringPrototypeIncludes,                                                 \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-string.prototype.indexof */                                      \
  TFJ(StringPrototypeIndexOf, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  /* ES6 #sec-string.prototype.italics */                                      \
  TFJ(StringPrototypeItalics, 0)                                               \
  /* ES6 #sec-string.prototype.lastindexof */                                  \
  CPP(StringPrototypeLastIndexOf)                                              \
  /* ES6 #sec-string.prototype.link */                                         \
  TFJ(StringPrototypeLink, 1, kValue)                                          \
  /* ES6 #sec-string.prototype.match */                                        \
  TFJ(StringPrototypeMatch, 1, kRegexp)                                        \
  /* ES #sec-string.prototype.matchAll */                                      \
  TFJ(StringPrototypeMatchAll, 1, kRegexp)                                     \
  /* ES6 #sec-string.prototype.localecompare */                                \
  CPP(StringPrototypeLocaleCompare)                                            \
  /* ES6 #sec-string.prototype.padEnd */                                       \
  TFJ(StringPrototypePadEnd, SharedFunctionInfo::kDontAdaptArgumentsSentinel)  \
  /* ES6 #sec-string.prototype.padStart */                                     \
  TFJ(StringPrototypePadStart,                                                 \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-string.prototype.repeat */                                       \
  TFJ(StringPrototypeRepeat, 1, kCount)                                        \
  /* ES6 #sec-string.prototype.replace */                                      \
  TFJ(StringPrototypeReplace, 2, kSearch, kReplace)                            \
  /* ES6 #sec-string.prototype.search */                                       \
  TFJ(StringPrototypeSearch, 1, kRegexp)                                       \
  /* ES6 #sec-string.prototype.slice */                                        \
  TFJ(StringPrototypeSlice, SharedFunctionInfo::kDontAdaptArgumentsSentinel)   \
  /* ES6 #sec-string.prototype.small */                                        \
  TFJ(StringPrototypeSmall, 0)                                                 \
  /* ES6 #sec-string.prototype.split */                                        \
  TFJ(StringPrototypeSplit, SharedFunctionInfo::kDontAdaptArgumentsSentinel)   \
  /* ES6 #sec-string.prototype.strike */                                       \
  TFJ(StringPrototypeStrike, 0)                                                \
  /* ES6 #sec-string.prototype.sub */                                          \
  TFJ(StringPrototypeSub, 0)                                                   \
  /* ES6 #sec-string.prototype.substr */                                       \
  TFJ(StringPrototypeSubstr, SharedFunctionInfo::kDontAdaptArgumentsSentinel)  \
  /* ES6 #sec-string.prototype.substring */                                    \
  TFJ(StringPrototypeSubstring,                                                \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-string.prototype.sup */                                          \
  TFJ(StringPrototypeSup, 0)                                                   \
  /* ES6 #sec-string.prototype.startswith */                                   \
  CPP(StringPrototypeStartsWith)                                               \
  /* ES6 #sec-string.prototype.tostring */                                     \
  TFJ(StringPrototypeToString, 0)                                              \
  TFJ(StringPrototypeTrim, SharedFunctionInfo::kDontAdaptArgumentsSentinel)    \
  TFJ(StringPrototypeTrimEnd, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  TFJ(StringPrototypeTrimStart,                                                \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-string.prototype.valueof */                                      \
  TFJ(StringPrototypeValueOf, 0)                                               \
  /* ES6 #sec-string.raw */                                                    \
  CPP(StringRaw)                                                               \
  /* ES6 #sec-string.prototype-@@iterator */                                   \
  TFJ(StringPrototypeIterator, 0)                                              \
                                                                               \
  /* StringIterator */                                                         \
  /* ES6 #sec-%stringiteratorprototype%.next */                                \
  TFJ(StringIteratorPrototypeNext, 0)                                          \
                                                                               \
  /* Symbol */                                                                 \
  /* ES #sec-symbol-constructor */                                             \
  CPP(SymbolConstructor)                                                       \
  /* ES6 #sec-symbol.for */                                                    \
  CPP(SymbolFor)                                                               \
  /* ES6 #sec-symbol.keyfor */                                                 \
  CPP(SymbolKeyFor)                                                            \
  /* ES6 #sec-symbol.prototype-@@toprimitive */                                \
  TFJ(SymbolPrototypeToPrimitive, 1, kHint)                                    \
  /* ES6 #sec-symbol.prototype.tostring */                                     \
  TFJ(SymbolPrototypeToString, 0)                                              \
  /* ES6 #sec-symbol.prototype.valueof */                                      \
  TFJ(SymbolPrototypeValueOf, 0)                                               \
                                                                               \
  /* TypedArray */                                                             \
  TFS(IterableToList, kIterable, kIteratorFn)                                  \
  TFS(TypedArrayInitialize, kHolder, kLength, kElementSize, kInitialize)       \
  TFS(TypedArrayInitializeWithBuffer, kHolder, kLength, kBuffer, kElementSize, \
      kByteOffset)                                                             \
  /* ES #sec-typedarray-constructors */                                        \
  TFS(CreateTypedArray, kTarget, kNewTarget, kArg1, kArg2, kArg3)              \
  TFJ(TypedArrayBaseConstructor, 0)                                            \
  TFJ(TypedArrayConstructorLazyDeoptContinuation, 1, kResult)                  \
  TFJ(TypedArrayConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)  \
  CPP(TypedArrayPrototypeBuffer)                                               \
  /* ES6 #sec-get-%typedarray%.prototype.bytelength */                         \
  TFJ(TypedArrayPrototypeByteLength, 0)                                        \
  /* ES6 #sec-get-%typedarray%.prototype.byteoffset */                         \
  TFJ(TypedArrayPrototypeByteOffset, 0)                                        \
  /* ES6 #sec-get-%typedarray%.prototype.length */                             \
  TFJ(TypedArrayPrototypeLength, 0)                                            \
  /* ES6 #sec-%typedarray%.prototype.entries */                                \
  TFJ(TypedArrayPrototypeEntries, 0)                                           \
  /* ES6 #sec-%typedarray%.prototype.keys */                                   \
  TFJ(TypedArrayPrototypeKeys, 0)                                              \
  /* ES6 #sec-%typedarray%.prototype.values */                                 \
  TFJ(TypedArrayPrototypeValues, 0)                                            \
  /* ES6 #sec-%typedarray%.prototype.copywithin */                             \
  CPP(TypedArrayPrototypeCopyWithin)                                           \
  /* ES6 #sec-%typedarray%.prototype.fill */                                   \
  CPP(TypedArrayPrototypeFill)                                                 \
  /* ES6 #sec-%typedarray%.prototype.filter */                                 \
  TFJ(TypedArrayPrototypeFilter,                                               \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 %TypedArray%.prototype.find */                                        \
  TFJ(TypedArrayPrototypeFind,                                                 \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 %TypedArray%.prototype.findIndex */                                   \
  TFJ(TypedArrayPrototypeFindIndex,                                            \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
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
  /* ES6 #sec-%typedarray%.prototype.slice */                                  \
  TFJ(TypedArrayPrototypeSlice,                                                \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 %TypedArray%.prototype.subarray */                                    \
  TFJ(TypedArrayPrototypeSubArray,                                             \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 #sec-get-%typedarray%.prototype-@@tostringtag */                      \
  TFJ(TypedArrayPrototypeToStringTag, 0)                                       \
  /* ES6 %TypedArray%.prototype.every */                                       \
  TFJ(TypedArrayPrototypeEvery,                                                \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 %TypedArray%.prototype.some */                                        \
  TFJ(TypedArrayPrototypeSome,                                                 \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 %TypedArray%.prototype.reduce */                                      \
  TFJ(TypedArrayPrototypeReduce,                                               \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 %TypedArray%.prototype.reduceRight */                                 \
  TFJ(TypedArrayPrototypeReduceRight,                                          \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 %TypedArray%.prototype.map */                                         \
  TFJ(TypedArrayPrototypeMap, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  /* ES6 %TypedArray%.prototype.forEach */                                     \
  TFJ(TypedArrayPrototypeForEach,                                              \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  /* ES6 %TypedArray%.of */                                                    \
  TFJ(TypedArrayOf, SharedFunctionInfo::kDontAdaptArgumentsSentinel)           \
  /* ES6 %TypedArray%.from */                                                  \
  TFJ(TypedArrayFrom, SharedFunctionInfo::kDontAdaptArgumentsSentinel)         \
                                                                               \
  /* Wasm */                                                                   \
  ASM(WasmCompileLazy)                                                         \
  TFC(WasmStackGuard, WasmRuntimeCall, 1)                                      \
  TFC(ThrowWasmTrapUnreachable, WasmRuntimeCall, 1)                            \
  TFC(ThrowWasmTrapMemOutOfBounds, WasmRuntimeCall, 1)                         \
  TFC(ThrowWasmTrapDivByZero, WasmRuntimeCall, 1)                              \
  TFC(ThrowWasmTrapDivUnrepresentable, WasmRuntimeCall, 1)                     \
  TFC(ThrowWasmTrapRemByZero, WasmRuntimeCall, 1)                              \
  TFC(ThrowWasmTrapFloatUnrepresentable, WasmRuntimeCall, 1)                   \
  TFC(ThrowWasmTrapFuncInvalid, WasmRuntimeCall, 1)                            \
  TFC(ThrowWasmTrapFuncSigMismatch, WasmRuntimeCall, 1)                        \
                                                                               \
  /* WeakMap */                                                                \
  TFJ(WeakMapConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
  TFS(WeakMapLookupHashIndex, kTable, kKey)                                    \
  TFJ(WeakMapGet, 1, kKey)                                                     \
  TFJ(WeakMapHas, 1, kKey)                                                     \
  TFJ(WeakMapPrototypeSet, 2, kKey, kValue)                                    \
  TFJ(WeakMapPrototypeDelete, 1, kKey)                                         \
                                                                               \
  /* WeakSet */                                                                \
  TFJ(WeakSetConstructor, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
  TFJ(WeakSetHas, 1, kKey)                                                     \
  TFJ(WeakSetPrototypeAdd, 1, kValue)                                          \
  TFJ(WeakSetPrototypeDelete, 1, kValue)                                       \
                                                                               \
  /* WeakSet / WeakMap Helpers */                                              \
  TFS(WeakCollectionDelete, kCollection, kKey)                                 \
  TFS(WeakCollectionSet, kCollection, kKey, kValue)                            \
                                                                               \
  /* AsyncGenerator */                                                         \
                                                                               \
  /* Await (proposal-async-iteration/#await), with resume behaviour */         \
  /* specific to Async Generators. Internal / Not exposed to JS code. */       \
  TFS(AsyncGeneratorAwaitCaught, kGenerator, kValue)                           \
  TFS(AsyncGeneratorAwaitUncaught, kGenerator, kValue)                         \
  TFC(AsyncGeneratorAwaitFulfill, PromiseReactionHandler, 1)                   \
  TFC(AsyncGeneratorAwaitReject, PromiseReactionHandler, 1)                    \
  TFC(AsyncGeneratorYieldFulfill, PromiseReactionHandler, 1)                   \
  TFC(AsyncGeneratorReturnClosedFulfill, PromiseReactionHandler, 1)            \
  TFC(AsyncGeneratorReturnClosedReject, PromiseReactionHandler, 1)             \
  TFC(AsyncGeneratorReturnFulfill, PromiseReactionHandler, 1)                  \
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
  /* Async-from-Sync Iterator */                                               \
                                                                               \
  /* %AsyncFromSyncIteratorPrototype% */                                       \
  /* See tc39.github.io/proposal-async-iteration/ */                           \
  /* #sec-%asyncfromsynciteratorprototype%-object) */                          \
  TFJ(AsyncFromSyncIteratorPrototypeNext, 1, kValue)                           \
  /* #sec-%asyncfromsynciteratorprototype%.throw */                            \
  TFJ(AsyncFromSyncIteratorPrototypeThrow, 1, kReason)                         \
  /* #sec-%asyncfromsynciteratorprototype%.return */                           \
  TFJ(AsyncFromSyncIteratorPrototypeReturn, 1, kValue)                         \
  /* #sec-async-iterator-value-unwrap-functions */                             \
  TFJ(AsyncIteratorValueUnwrap, 1, kValue)

#ifdef V8_INTL_SUPPORT
#define BUILTIN_LIST(CPP, API, TFJ, TFC, TFS, TFH, ASM)        \
  BUILTIN_LIST_BASE(CPP, API, TFJ, TFC, TFS, TFH, ASM)         \
                                                               \
  TFS(StringToLowerCaseIntl, kString)                          \
  /* ES #sec-string.prototype.tolowercase */                   \
  TFJ(StringPrototypeToLowerCaseIntl, 0)                       \
  /* ES #sec-string.prototype.touppercase */                   \
  CPP(StringPrototypeToUpperCaseIntl)                          \
  /* ES #sec-string.prototype.normalize */                     \
  CPP(StringPrototypeNormalizeIntl)                            \
  /* ecma402 #sec-intl.numberformat.prototype.formattoparts */ \
  CPP(NumberFormatPrototypeFormatToParts)
#else
#define BUILTIN_LIST(CPP, API, TFJ, TFC, TFS, TFH, ASM) \
  BUILTIN_LIST_BASE(CPP, API, TFJ, TFC, TFS, TFH, ASM)  \
                                                        \
  /* no-op fallback version */                          \
  CPP(StringPrototypeNormalize)                         \
  /* same as toLowercase; fallback version */           \
  CPP(StringPrototypeToLocaleLowerCase)                 \
  /* same as toUppercase; fallback version */           \
  CPP(StringPrototypeToLocaleUpperCase)                 \
  /* (obsolete) Unibrow version */                      \
  CPP(StringPrototypeToLowerCase)                       \
  /* (obsolete) Unibrow version */                      \
  CPP(StringPrototypeToUpperCase)
#endif  // V8_INTL_SUPPORT

// The exception thrown in the following builtins are caught
// internally and result in a promise rejection.
#define BUILTIN_PROMISE_REJECTION_PREDICTION_LIST(V) \
  V(AsyncFromSyncIteratorPrototypeNext)              \
  V(AsyncFromSyncIteratorPrototypeReturn)            \
  V(AsyncFromSyncIteratorPrototypeThrow)             \
  V(AsyncGeneratorResolve)                           \
  V(PromiseAll)                                      \
  V(PromiseConstructor)                              \
  V(PromiseConstructorLazyDeoptContinuation)         \
  V(PromiseFulfillReactionJob)                       \
  V(PromiseRace)                                     \
  V(ResolvePromise)

// The exception thrown in the following builtins are caught internally and will
// not be propagated further or re-thrown
#define BUILTIN_EXCEPTION_CAUGHT_PREDICTION_LIST(V) V(PromiseRejectReactionJob)

#define IGNORE_BUILTIN(...)

#define BUILTIN_LIST_ALL(V) BUILTIN_LIST(V, V, V, V, V, V, V)

#define BUILTIN_LIST_C(V)                                            \
  BUILTIN_LIST(V, V, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_A(V)                                                      \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, V)

#define BUILTIN_LIST_TFS(V)                                                    \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               V, IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFJ(V)                                       \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, V, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_TFC(V)                                       \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, V, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN)

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_DEFINITIONS_H_
