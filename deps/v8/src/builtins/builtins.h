// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_H_
#define V8_BUILTINS_BUILTINS_H_

#include "src/base/flags.h"
#include "src/handles.h"

namespace v8 {
namespace internal {

#define CODE_AGE_LIST_WITH_ARG(V, A) \
  V(Quadragenarian, A)               \
  V(Quinquagenarian, A)              \
  V(Sexagenarian, A)                 \
  V(Septuagenarian, A)               \
  V(Octogenarian, A)

#define CODE_AGE_LIST_IGNORE_ARG(X, V) V(X)

#define CODE_AGE_LIST(V) CODE_AGE_LIST_WITH_ARG(CODE_AGE_LIST_IGNORE_ARG, V)

#define CODE_AGE_LIST_COMPLETE(V) \
  V(ToBeExecutedOnce)             \
  V(NotExecuted)                  \
  V(ExecutedOnce)                 \
  V(NoAge)                        \
  CODE_AGE_LIST_WITH_ARG(CODE_AGE_LIST_IGNORE_ARG, V)

#define DECLARE_CODE_AGE_BUILTIN(C, V) V(Make##C##CodeYoungAgain)

// CPP: Builtin in C++. Entered via BUILTIN_EXIT frame.
//      Args: name
// API: Builtin in C++ for API callbacks. Entered via EXIT frame.
//      Args: name
// TFJ: Builtin in Turbofan, with JS linkage (callable as Javascript function).
//      Args: name, arguments count
// TFS: Builtin in Turbofan, with CodeStub linkage.
//      Args: name, code kind, extra IC state, interface descriptor
// ASM: Builtin in platform-dependent assembly.
//      Args: name
// ASH: Handlers implemented in platform-dependent assembly.
//      Args: name, code kind, extra IC state
// DBG: Builtin in platform-dependent assembly, used by the debugger.
//      Args: name
#define BUILTIN_LIST(CPP, API, TFJ, TFS, ASM, ASH, DBG)                        \
  ASM(Abort)                                                                   \
  /* Code aging */                                                             \
  CODE_AGE_LIST_WITH_ARG(DECLARE_CODE_AGE_BUILTIN, ASM)                        \
                                                                               \
  /* Declared first for dependency reasons */                                  \
  ASM(CompileLazy)                                                             \
  TFS(ToObject, BUILTIN, kNoExtraICState, TypeConversion)                      \
  TFS(FastNewObject, BUILTIN, kNoExtraICState, FastNewObject)                  \
                                                                               \
  /* Calls */                                                                  \
  ASM(ArgumentsAdaptorTrampoline)                                              \
  /* ES6 section 9.2.1 [[Call]] ( thisArgument, argumentsList) */              \
  ASM(CallFunction_ReceiverIsNullOrUndefined)                                  \
  ASM(CallFunction_ReceiverIsNotNullOrUndefined)                               \
  ASM(CallFunction_ReceiverIsAny)                                              \
  ASM(TailCallFunction_ReceiverIsNullOrUndefined)                              \
  ASM(TailCallFunction_ReceiverIsNotNullOrUndefined)                           \
  ASM(TailCallFunction_ReceiverIsAny)                                          \
  /* ES6 section 9.4.1.1 [[Call]] ( thisArgument, argumentsList) */            \
  ASM(CallBoundFunction)                                                       \
  ASM(TailCallBoundFunction)                                                   \
  /* ES6 section 7.3.12 Call(F, V, [argumentsList]) */                         \
  ASM(Call_ReceiverIsNullOrUndefined)                                          \
  ASM(Call_ReceiverIsNotNullOrUndefined)                                       \
  ASM(Call_ReceiverIsAny)                                                      \
  ASM(TailCall_ReceiverIsNullOrUndefined)                                      \
  ASM(TailCall_ReceiverIsNotNullOrUndefined)                                   \
  ASM(TailCall_ReceiverIsAny)                                                  \
                                                                               \
  /* Construct */                                                              \
  /* ES6 section 9.2.2 [[Construct]] ( argumentsList, newTarget) */            \
  ASM(ConstructFunction)                                                       \
  /* ES6 section 9.4.1.2 [[Construct]] (argumentsList, newTarget) */           \
  ASM(ConstructBoundFunction)                                                  \
  ASM(ConstructedNonConstructable)                                             \
  /* ES6 section 9.5.14 [[Construct]] ( argumentsList, newTarget) */           \
  ASM(ConstructProxy)                                                          \
  /* ES6 section 7.3.13 Construct (F, [argumentsList], [newTarget]) */         \
  ASM(Construct)                                                               \
  ASM(JSConstructStubApi)                                                      \
  ASM(JSConstructStubGeneric)                                                  \
  ASM(JSBuiltinsConstructStub)                                                 \
  ASM(JSBuiltinsConstructStubForDerived)                                       \
  TFS(FastNewClosure, BUILTIN, kNoExtraICState, FastNewClosure)                \
  TFS(FastNewFunctionContextEval, BUILTIN, kNoExtraICState,                    \
      FastNewFunctionContext)                                                  \
  TFS(FastNewFunctionContextFunction, BUILTIN, kNoExtraICState,                \
      FastNewFunctionContext)                                                  \
  TFS(FastCloneRegExp, BUILTIN, kNoExtraICState, FastCloneRegExp)              \
  TFS(FastCloneShallowArrayTrack, BUILTIN, kNoExtraICState,                    \
      FastCloneShallowArray)                                                   \
  TFS(FastCloneShallowArrayDontTrack, BUILTIN, kNoExtraICState,                \
      FastCloneShallowArray)                                                   \
  TFS(FastCloneShallowObject0, BUILTIN, kNoExtraICState,                       \
      FastCloneShallowObject)                                                  \
  TFS(FastCloneShallowObject1, BUILTIN, kNoExtraICState,                       \
      FastCloneShallowObject)                                                  \
  TFS(FastCloneShallowObject2, BUILTIN, kNoExtraICState,                       \
      FastCloneShallowObject)                                                  \
  TFS(FastCloneShallowObject3, BUILTIN, kNoExtraICState,                       \
      FastCloneShallowObject)                                                  \
  TFS(FastCloneShallowObject4, BUILTIN, kNoExtraICState,                       \
      FastCloneShallowObject)                                                  \
  TFS(FastCloneShallowObject5, BUILTIN, kNoExtraICState,                       \
      FastCloneShallowObject)                                                  \
  TFS(FastCloneShallowObject6, BUILTIN, kNoExtraICState,                       \
      FastCloneShallowObject)                                                  \
                                                                               \
  /* Apply and entries */                                                      \
  ASM(Apply)                                                                   \
  ASM(JSEntryTrampoline)                                                       \
  ASM(JSConstructEntryTrampoline)                                              \
  ASM(ResumeGeneratorTrampoline)                                               \
                                                                               \
  /* Stack and interrupt check */                                              \
  ASM(InterruptCheck)                                                          \
  ASM(StackCheck)                                                              \
                                                                               \
  /* String helpers */                                                         \
  TFS(StringEqual, BUILTIN, kNoExtraICState, Compare)                          \
  TFS(StringNotEqual, BUILTIN, kNoExtraICState, Compare)                       \
  TFS(StringLessThan, BUILTIN, kNoExtraICState, Compare)                       \
  TFS(StringLessThanOrEqual, BUILTIN, kNoExtraICState, Compare)                \
  TFS(StringGreaterThan, BUILTIN, kNoExtraICState, Compare)                    \
  TFS(StringGreaterThanOrEqual, BUILTIN, kNoExtraICState, Compare)             \
  TFS(StringCharAt, BUILTIN, kNoExtraICState, StringCharAt)                    \
  TFS(StringCharCodeAt, BUILTIN, kNoExtraICState, StringCharCodeAt)            \
                                                                               \
  /* Interpreter */                                                            \
  ASM(InterpreterEntryTrampoline)                                              \
  ASM(InterpreterPushArgsAndCall)                                              \
  ASM(InterpreterPushArgsAndCallFunction)                                      \
  ASM(InterpreterPushArgsAndTailCall)                                          \
  ASM(InterpreterPushArgsAndTailCallFunction)                                  \
  ASM(InterpreterPushArgsAndConstruct)                                         \
  ASM(InterpreterPushArgsAndConstructFunction)                                 \
  ASM(InterpreterPushArgsAndConstructArray)                                    \
  ASM(InterpreterEnterBytecodeAdvance)                                         \
  ASM(InterpreterEnterBytecodeDispatch)                                        \
  ASM(InterpreterOnStackReplacement)                                           \
                                                                               \
  /* Code life-cycle */                                                        \
  ASM(CompileBaseline)                                                         \
  ASM(CompileOptimized)                                                        \
  ASM(CompileOptimizedConcurrent)                                              \
  ASM(InOptimizationQueue)                                                     \
  ASM(InstantiateAsmJs)                                                        \
  ASM(MarkCodeAsToBeExecutedOnce)                                              \
  ASM(MarkCodeAsExecutedOnce)                                                  \
  ASM(MarkCodeAsExecutedTwice)                                                 \
  ASM(NotifyDeoptimized)                                                       \
  ASM(NotifySoftDeoptimized)                                                   \
  ASM(NotifyLazyDeoptimized)                                                   \
  ASM(NotifyStubFailure)                                                       \
  ASM(NotifyStubFailureSaveDoubles)                                            \
  ASM(OnStackReplacement)                                                      \
                                                                               \
  /* API callback handling */                                                  \
  API(HandleApiCall)                                                           \
  API(HandleApiCallAsFunction)                                                 \
  API(HandleApiCallAsConstructor)                                              \
  ASM(HandleFastApiCall)                                                       \
                                                                               \
  /* Adapters for Turbofan into runtime */                                     \
  ASM(AllocateInNewSpace)                                                      \
  ASM(AllocateInOldSpace)                                                      \
                                                                               \
  /* TurboFan support builtins */                                              \
  TFS(CopyFastSmiOrObjectElements, BUILTIN, kNoExtraICState,                   \
      CopyFastSmiOrObjectElements)                                             \
  TFS(GrowFastDoubleElements, BUILTIN, kNoExtraICState, GrowArrayElements)     \
  TFS(GrowFastSmiOrObjectElements, BUILTIN, kNoExtraICState,                   \
      GrowArrayElements)                                                       \
  TFS(NewUnmappedArgumentsElements, BUILTIN, kNoExtraICState,                  \
      NewArgumentsElements)                                                    \
  TFS(NewRestParameterElements, BUILTIN, kNoExtraICState,                      \
      NewArgumentsElements)                                                    \
                                                                               \
  /* Debugger */                                                               \
  DBG(FrameDropper_LiveEdit)                                                   \
  DBG(Return_DebugBreak)                                                       \
  DBG(Slot_DebugBreak)                                                         \
                                                                               \
  /* Type conversions */                                                       \
  TFS(ToBoolean, BUILTIN, kNoExtraICState, TypeConversion)                     \
  TFS(OrdinaryToPrimitive_Number, BUILTIN, kNoExtraICState, TypeConversion)    \
  TFS(OrdinaryToPrimitive_String, BUILTIN, kNoExtraICState, TypeConversion)    \
  TFS(NonPrimitiveToPrimitive_Default, BUILTIN, kNoExtraICState,               \
      TypeConversion)                                                          \
  TFS(NonPrimitiveToPrimitive_Number, BUILTIN, kNoExtraICState,                \
      TypeConversion)                                                          \
  TFS(NonPrimitiveToPrimitive_String, BUILTIN, kNoExtraICState,                \
      TypeConversion)                                                          \
  TFS(StringToNumber, BUILTIN, kNoExtraICState, TypeConversion)                \
  TFS(ToName, BUILTIN, kNoExtraICState, TypeConversion)                        \
  TFS(NonNumberToNumber, BUILTIN, kNoExtraICState, TypeConversion)             \
  TFS(ToNumber, BUILTIN, kNoExtraICState, TypeConversion)                      \
  TFS(ToString, BUILTIN, kNoExtraICState, TypeConversion)                      \
  TFS(ToInteger, BUILTIN, kNoExtraICState, TypeConversion)                     \
  TFS(ToLength, BUILTIN, kNoExtraICState, TypeConversion)                      \
  TFS(Typeof, BUILTIN, kNoExtraICState, Typeof)                                \
  TFS(GetSuperConstructor, BUILTIN, kNoExtraICState, TypeConversion)           \
                                                                               \
  /* Handlers */                                                               \
  TFS(KeyedLoadIC_Megamorphic_TF, KEYED_LOAD_IC, kNoExtraICState,              \
      LoadWithVector)                                                          \
  TFS(KeyedLoadIC_Miss, BUILTIN, kNoExtraICState, LoadWithVector)              \
  TFS(KeyedLoadIC_Slow, HANDLER, Code::KEYED_LOAD_IC, LoadWithVector)          \
  TFS(KeyedStoreIC_Megamorphic_TF, KEYED_STORE_IC, kNoExtraICState,            \
      StoreWithVector)                                                         \
  TFS(KeyedStoreIC_Megamorphic_Strict_TF, KEYED_STORE_IC,                      \
      StoreICState::kStrictModeState, StoreWithVector)                         \
  ASM(KeyedStoreIC_Miss)                                                       \
  ASH(KeyedStoreIC_Slow, HANDLER, Code::KEYED_STORE_IC)                        \
  TFS(LoadGlobalIC_Miss, BUILTIN, kNoExtraICState, LoadGlobalWithVector)       \
  TFS(LoadGlobalIC_Slow, HANDLER, Code::LOAD_GLOBAL_IC, LoadGlobalWithVector)  \
  ASH(LoadIC_Getter_ForDeopt, LOAD_IC, kNoExtraICState)                        \
  TFS(LoadIC_Miss, BUILTIN, kNoExtraICState, LoadWithVector)                   \
  TFS(LoadIC_Normal, HANDLER, Code::LOAD_IC, LoadWithVector)                   \
  TFS(LoadIC_Slow, HANDLER, Code::LOAD_IC, LoadWithVector)                     \
  TFS(StoreIC_Miss, BUILTIN, kNoExtraICState, StoreWithVector)                 \
  TFS(StoreIC_Normal, HANDLER, Code::STORE_IC, StoreWithVector)                \
  ASH(StoreIC_Setter_ForDeopt, STORE_IC, StoreICState::kStrictModeState)       \
  TFS(StoreIC_SlowSloppy, HANDLER, Code::STORE_IC, StoreWithVector)            \
  TFS(StoreIC_SlowStrict, HANDLER, Code::STORE_IC, StoreWithVector)            \
                                                                               \
  /* Built-in functions for Javascript */                                      \
  /* Special internal builtins */                                              \
  CPP(EmptyFunction)                                                           \
  CPP(Illegal)                                                                 \
  CPP(RestrictedFunctionPropertiesThrower)                                     \
  CPP(RestrictedStrictArgumentsPropertiesThrower)                              \
  CPP(UnsupportedThrower)                                                      \
  TFJ(ReturnReceiver, 0)                                                       \
                                                                               \
  /* Array */                                                                  \
  ASM(ArrayCode)                                                               \
  ASM(InternalArrayCode)                                                       \
  CPP(ArrayConcat)                                                             \
  /* ES6 section 22.1.2.2 Array.isArray */                                     \
  TFJ(ArrayIsArray, 1)                                                         \
  /* ES7 #sec-array.prototype.includes */                                      \
  TFJ(ArrayIncludes, 2)                                                        \
  TFJ(ArrayIndexOf, 2)                                                         \
  CPP(ArrayPop)                                                                \
  CPP(ArrayPush)                                                               \
  TFJ(FastArrayPush, -1)                                                       \
  CPP(ArrayShift)                                                              \
  CPP(ArraySlice)                                                              \
  CPP(ArraySplice)                                                             \
  CPP(ArrayUnshift)                                                            \
  /* ES6 #sec-array.prototype.entries */                                       \
  TFJ(ArrayPrototypeEntries, 0)                                                \
  /* ES6 #sec-array.prototype.keys */                                          \
  TFJ(ArrayPrototypeKeys, 0)                                                   \
  /* ES6 #sec-array.prototype.values */                                        \
  TFJ(ArrayPrototypeValues, 0)                                                 \
  /* ES6 #sec-%arrayiteratorprototype%.next */                                 \
  TFJ(ArrayIteratorPrototypeNext, 0)                                           \
                                                                               \
  /* ArrayBuffer */                                                            \
  CPP(ArrayBufferConstructor)                                                  \
  CPP(ArrayBufferConstructor_ConstructStub)                                    \
  CPP(ArrayBufferPrototypeGetByteLength)                                       \
  CPP(ArrayBufferIsView)                                                       \
                                                                               \
  /* Boolean */                                                                \
  CPP(BooleanConstructor)                                                      \
  CPP(BooleanConstructor_ConstructStub)                                        \
  /* ES6 section 19.3.3.2 Boolean.prototype.toString ( ) */                    \
  TFJ(BooleanPrototypeToString, 0)                                             \
  /* ES6 section 19.3.3.3 Boolean.prototype.valueOf ( ) */                     \
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
  /* DataView */                                                               \
  CPP(DataViewConstructor)                                                     \
  CPP(DataViewConstructor_ConstructStub)                                       \
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
                                                                               \
  /* Date */                                                                   \
  CPP(DateConstructor)                                                         \
  CPP(DateConstructor_ConstructStub)                                           \
  /* ES6 section 20.3.4.2 Date.prototype.getDate ( ) */                        \
  TFJ(DatePrototypeGetDate, 0)                                                 \
  /* ES6 section 20.3.4.3 Date.prototype.getDay ( ) */                         \
  TFJ(DatePrototypeGetDay, 0)                                                  \
  /* ES6 section 20.3.4.4 Date.prototype.getFullYear ( ) */                    \
  TFJ(DatePrototypeGetFullYear, 0)                                             \
  /* ES6 section 20.3.4.5 Date.prototype.getHours ( ) */                       \
  TFJ(DatePrototypeGetHours, 0)                                                \
  /* ES6 section 20.3.4.6 Date.prototype.getMilliseconds ( ) */                \
  TFJ(DatePrototypeGetMilliseconds, 0)                                         \
  /* ES6 section 20.3.4.7 Date.prototype.getMinutes ( ) */                     \
  TFJ(DatePrototypeGetMinutes, 0)                                              \
  /* ES6 section 20.3.4.8 Date.prototype.getMonth */                           \
  TFJ(DatePrototypeGetMonth, 0)                                                \
  /* ES6 section 20.3.4.9 Date.prototype.getSeconds ( ) */                     \
  TFJ(DatePrototypeGetSeconds, 0)                                              \
  /* ES6 section 20.3.4.10 Date.prototype.getTime ( ) */                       \
  TFJ(DatePrototypeGetTime, 0)                                                 \
  /* ES6 section 20.3.4.11 Date.prototype.getTimezoneOffset ( ) */             \
  TFJ(DatePrototypeGetTimezoneOffset, 0)                                       \
  /* ES6 section 20.3.4.12 Date.prototype.getUTCDate ( ) */                    \
  TFJ(DatePrototypeGetUTCDate, 0)                                              \
  /* ES6 section 20.3.4.13 Date.prototype.getUTCDay ( ) */                     \
  TFJ(DatePrototypeGetUTCDay, 0)                                               \
  /* ES6 section 20.3.4.14 Date.prototype.getUTCFullYear ( ) */                \
  TFJ(DatePrototypeGetUTCFullYear, 0)                                          \
  /* ES6 section 20.3.4.15 Date.prototype.getUTCHours ( ) */                   \
  TFJ(DatePrototypeGetUTCHours, 0)                                             \
  /* ES6 section 20.3.4.16 Date.prototype.getUTCMilliseconds ( ) */            \
  TFJ(DatePrototypeGetUTCMilliseconds, 0)                                      \
  /* ES6 section 20.3.4.17 Date.prototype.getUTCMinutes ( ) */                 \
  TFJ(DatePrototypeGetUTCMinutes, 0)                                           \
  /* ES6 section 20.3.4.18 Date.prototype.getUTCMonth ( ) */                   \
  TFJ(DatePrototypeGetUTCMonth, 0)                                             \
  /* ES6 section 20.3.4.19 Date.prototype.getUTCSeconds ( ) */                 \
  TFJ(DatePrototypeGetUTCSeconds, 0)                                           \
  /* ES6 section 20.3.4.44 Date.prototype.valueOf ( ) */                       \
  TFJ(DatePrototypeValueOf, 0)                                                 \
  /* ES6 section 20.3.4.45 Date.prototype [ @@toPrimitive ] ( hint ) */        \
  TFJ(DatePrototypeToPrimitive, 1)                                             \
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
  TFJ(FastFunctionPrototypeBind,                                               \
      SharedFunctionInfo::kDontAdaptArgumentsSentinel)                         \
  ASM(FunctionPrototypeCall)                                                   \
  /* ES6 section 19.2.3.6 Function.prototype [ @@hasInstance ] ( V ) */        \
  TFJ(FunctionPrototypeHasInstance, 1)                                         \
  CPP(FunctionPrototypeToString)                                               \
                                                                               \
  /* Belongs to Objects but is a dependency of GeneratorPrototypeResume */     \
  TFS(CreateIterResultObject, BUILTIN, kNoExtraICState,                        \
      CreateIterResultObject)                                                  \
                                                                               \
  /* Generator and Async */                                                    \
  CPP(GeneratorFunctionConstructor)                                            \
  /* ES6 section 25.3.1.2 Generator.prototype.next ( value ) */                \
  TFJ(GeneratorPrototypeNext, 1)                                               \
  /* ES6 section 25.3.1.3 Generator.prototype.return ( value ) */              \
  TFJ(GeneratorPrototypeReturn, 1)                                             \
  /* ES6 section 25.3.1.4 Generator.prototype.throw ( exception ) */           \
  TFJ(GeneratorPrototypeThrow, 1)                                              \
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
  /* ES6 section 18.2.2 isFinite ( number ) */                                 \
  TFJ(GlobalIsFinite, 1)                                                       \
  /* ES6 section 18.2.3 isNaN ( number ) */                                    \
  TFJ(GlobalIsNaN, 1)                                                          \
                                                                               \
  /* JSON */                                                                   \
  CPP(JsonParse)                                                               \
  CPP(JsonStringify)                                                           \
                                                                               \
  /* ICs */                                                                    \
  TFS(LoadIC, LOAD_IC, kNoExtraICState, LoadWithVector)                        \
  TFS(LoadICTrampoline, LOAD_IC, kNoExtraICState, Load)                        \
  TFS(KeyedLoadIC, KEYED_LOAD_IC, kNoExtraICState, LoadWithVector)             \
  TFS(KeyedLoadICTrampoline, KEYED_LOAD_IC, kNoExtraICState, Load)             \
  TFS(StoreIC, STORE_IC, kNoExtraICState, StoreWithVector)                     \
  TFS(StoreICTrampoline, STORE_IC, kNoExtraICState, Store)                     \
  TFS(StoreICStrict, STORE_IC, StoreICState::kStrictModeState,                 \
      StoreWithVector)                                                         \
  TFS(StoreICStrictTrampoline, STORE_IC, StoreICState::kStrictModeState,       \
      Store)                                                                   \
  TFS(KeyedStoreIC, KEYED_STORE_IC, kNoExtraICState, StoreWithVector)          \
  TFS(KeyedStoreICTrampoline, KEYED_STORE_IC, kNoExtraICState, Store)          \
  TFS(KeyedStoreICStrict, KEYED_STORE_IC, StoreICState::kStrictModeState,      \
      StoreWithVector)                                                         \
  TFS(KeyedStoreICStrictTrampoline, KEYED_STORE_IC,                            \
      StoreICState::kStrictModeState, Store)                                   \
  TFS(LoadGlobalIC, LOAD_GLOBAL_IC, LoadGlobalICState::kNotInsideTypeOfState,  \
      LoadGlobalWithVector)                                                    \
  TFS(LoadGlobalICInsideTypeof, LOAD_GLOBAL_IC,                                \
      LoadGlobalICState::kInsideTypeOfState, LoadGlobalWithVector)             \
  TFS(LoadGlobalICTrampoline, LOAD_GLOBAL_IC,                                  \
      LoadGlobalICState::kNotInsideTypeOfState, LoadGlobal)                    \
  TFS(LoadGlobalICInsideTypeofTrampoline, LOAD_GLOBAL_IC,                      \
      LoadGlobalICState::kInsideTypeOfState, LoadGlobal)                       \
                                                                               \
  /* Math */                                                                   \
  /* ES6 section 20.2.2.1 Math.abs ( x ) */                                    \
  TFJ(MathAbs, 1)                                                              \
  /* ES6 section 20.2.2.2 Math.acos ( x ) */                                   \
  TFJ(MathAcos, 1)                                                             \
  /* ES6 section 20.2.2.3 Math.acosh ( x ) */                                  \
  TFJ(MathAcosh, 1)                                                            \
  /* ES6 section 20.2.2.4 Math.asin ( x ) */                                   \
  TFJ(MathAsin, 1)                                                             \
  /* ES6 section 20.2.2.5 Math.asinh ( x ) */                                  \
  TFJ(MathAsinh, 1)                                                            \
  /* ES6 section 20.2.2.6 Math.atan ( x ) */                                   \
  TFJ(MathAtan, 1)                                                             \
  /* ES6 section 20.2.2.7 Math.atanh ( x ) */                                  \
  TFJ(MathAtanh, 1)                                                            \
  /* ES6 section 20.2.2.8 Math.atan2 ( y, x ) */                               \
  TFJ(MathAtan2, 2)                                                            \
  /* ES6 section 20.2.2.9 Math.cbrt ( x ) */                                   \
  TFJ(MathCbrt, 1)                                                             \
  /* ES6 section 20.2.2.10 Math.ceil ( x ) */                                  \
  TFJ(MathCeil, 1)                                                             \
  /* ES6 section 20.2.2.11 Math.clz32 ( x ) */                                 \
  TFJ(MathClz32, 1)                                                            \
  /* ES6 section 20.2.2.12 Math.cos ( x ) */                                   \
  TFJ(MathCos, 1)                                                              \
  /* ES6 section 20.2.2.13 Math.cosh ( x ) */                                  \
  TFJ(MathCosh, 1)                                                             \
  /* ES6 section 20.2.2.14 Math.exp ( x ) */                                   \
  TFJ(MathExp, 1)                                                              \
  /* ES6 section 20.2.2.15 Math.expm1 ( x ) */                                 \
  TFJ(MathExpm1, 1)                                                            \
  /* ES6 section 20.2.2.16 Math.floor ( x ) */                                 \
  TFJ(MathFloor, 1)                                                            \
  /* ES6 section 20.2.2.17 Math.fround ( x ) */                                \
  TFJ(MathFround, 1)                                                           \
  /* ES6 section 20.2.2.18 Math.hypot ( value1, value2, ...values ) */         \
  CPP(MathHypot)                                                               \
  /* ES6 section 20.2.2.19 Math.imul ( x, y ) */                               \
  TFJ(MathImul, 2)                                                             \
  /* ES6 section 20.2.2.20 Math.log ( x ) */                                   \
  TFJ(MathLog, 1)                                                              \
  /* ES6 section 20.2.2.21 Math.log1p ( x ) */                                 \
  TFJ(MathLog1p, 1)                                                            \
  /* ES6 section 20.2.2.22 Math.log10 ( x ) */                                 \
  TFJ(MathLog10, 1)                                                            \
  /* ES6 section 20.2.2.23 Math.log2 ( x ) */                                  \
  TFJ(MathLog2, 1)                                                             \
  /* ES6 section 20.2.2.24 Math.max ( value1, value2 , ...values ) */          \
  ASM(MathMax)                                                                 \
  /* ES6 section 20.2.2.25 Math.min ( value1, value2 , ...values ) */          \
  ASM(MathMin)                                                                 \
  /* ES6 section 20.2.2.26 Math.pow ( x, y ) */                                \
  TFJ(MathPow, 2)                                                              \
  /* ES6 section 20.2.2.27 Math.random */                                      \
  TFJ(MathRandom, 0)                                                           \
  /* ES6 section 20.2.2.28 Math.round ( x ) */                                 \
  TFJ(MathRound, 1)                                                            \
  /* ES6 section 20.2.2.29 Math.sign ( x ) */                                  \
  TFJ(MathSign, 1)                                                             \
  /* ES6 section 20.2.2.30 Math.sin ( x ) */                                   \
  TFJ(MathSin, 1)                                                              \
  /* ES6 section 20.2.2.31 Math.sinh ( x ) */                                  \
  TFJ(MathSinh, 1)                                                             \
  /* ES6 section 20.2.2.32 Math.sqrt ( x ) */                                  \
  TFJ(MathTan, 1)                                                              \
  /* ES6 section 20.2.2.33 Math.tan ( x ) */                                   \
  TFJ(MathTanh, 1)                                                             \
  /* ES6 section 20.2.2.34 Math.tanh ( x ) */                                  \
  TFJ(MathSqrt, 1)                                                             \
  /* ES6 section 20.2.2.35 Math.trunc ( x ) */                                 \
  TFJ(MathTrunc, 1)                                                            \
                                                                               \
  /* Number */                                                                 \
  /* ES6 section 20.1.1.1 Number ( [ value ] ) for the [[Call]] case */        \
  ASM(NumberConstructor)                                                       \
  /* ES6 section 20.1.1.1 Number ( [ value ] ) for the [[Construct]] case */   \
  ASM(NumberConstructor_ConstructStub)                                         \
  /* ES6 section 20.1.2.2 Number.isFinite ( number ) */                        \
  TFJ(NumberIsFinite, 1)                                                       \
  /* ES6 section 20.1.2.3 Number.isInteger ( number ) */                       \
  TFJ(NumberIsInteger, 1)                                                      \
  /* ES6 section 20.1.2.4 Number.isNaN ( number ) */                           \
  TFJ(NumberIsNaN, 1)                                                          \
  /* ES6 section 20.1.2.5 Number.isSafeInteger ( number ) */                   \
  TFJ(NumberIsSafeInteger, 1)                                                  \
  /* ES6 section 20.1.2.12 Number.parseFloat ( string ) */                     \
  TFJ(NumberParseFloat, 1)                                                     \
  /* ES6 section 20.1.2.13 Number.parseInt ( string, radix ) */                \
  TFJ(NumberParseInt, 2)                                                       \
  CPP(NumberPrototypeToExponential)                                            \
  CPP(NumberPrototypeToFixed)                                                  \
  CPP(NumberPrototypeToLocaleString)                                           \
  CPP(NumberPrototypeToPrecision)                                              \
  CPP(NumberPrototypeToString)                                                 \
  /* ES6 section 20.1.3.7 Number.prototype.valueOf ( ) */                      \
  TFJ(NumberPrototypeValueOf, 0)                                               \
  TFS(Add, BUILTIN, kNoExtraICState, BinaryOp)                                 \
  TFS(Subtract, BUILTIN, kNoExtraICState, BinaryOp)                            \
  TFS(Multiply, BUILTIN, kNoExtraICState, BinaryOp)                            \
  TFS(Divide, BUILTIN, kNoExtraICState, BinaryOp)                              \
  TFS(Modulus, BUILTIN, kNoExtraICState, BinaryOp)                             \
  TFS(BitwiseAnd, BUILTIN, kNoExtraICState, BinaryOp)                          \
  TFS(BitwiseOr, BUILTIN, kNoExtraICState, BinaryOp)                           \
  TFS(BitwiseXor, BUILTIN, kNoExtraICState, BinaryOp)                          \
  TFS(ShiftLeft, BUILTIN, kNoExtraICState, BinaryOp)                           \
  TFS(ShiftRight, BUILTIN, kNoExtraICState, BinaryOp)                          \
  TFS(ShiftRightLogical, BUILTIN, kNoExtraICState, BinaryOp)                   \
  TFS(LessThan, BUILTIN, kNoExtraICState, Compare)                             \
  TFS(LessThanOrEqual, BUILTIN, kNoExtraICState, Compare)                      \
  TFS(GreaterThan, BUILTIN, kNoExtraICState, Compare)                          \
  TFS(GreaterThanOrEqual, BUILTIN, kNoExtraICState, Compare)                   \
  TFS(Equal, BUILTIN, kNoExtraICState, Compare)                                \
  TFS(NotEqual, BUILTIN, kNoExtraICState, Compare)                             \
  TFS(StrictEqual, BUILTIN, kNoExtraICState, Compare)                          \
  TFS(StrictNotEqual, BUILTIN, kNoExtraICState, Compare)                       \
                                                                               \
  /* Object */                                                                 \
  CPP(ObjectAssign)                                                            \
  TFJ(ObjectCreate, 2)                                                         \
  CPP(ObjectDefineGetter)                                                      \
  CPP(ObjectDefineProperties)                                                  \
  CPP(ObjectDefineProperty)                                                    \
  CPP(ObjectDefineSetter)                                                      \
  CPP(ObjectEntries)                                                           \
  CPP(ObjectFreeze)                                                            \
  CPP(ObjectGetOwnPropertyDescriptor)                                          \
  CPP(ObjectGetOwnPropertyDescriptors)                                         \
  CPP(ObjectGetOwnPropertyNames)                                               \
  CPP(ObjectGetOwnPropertySymbols)                                             \
  CPP(ObjectGetPrototypeOf)                                                    \
  CPP(ObjectSetPrototypeOf)                                                    \
  /* ES6 section 19.1.3.2 Object.prototype.hasOwnProperty */                   \
  TFJ(ObjectHasOwnProperty, 1)                                                 \
  CPP(ObjectIs)                                                                \
  CPP(ObjectIsExtensible)                                                      \
  CPP(ObjectIsFrozen)                                                          \
  CPP(ObjectIsSealed)                                                          \
  CPP(ObjectKeys)                                                              \
  CPP(ObjectLookupGetter)                                                      \
  CPP(ObjectLookupSetter)                                                      \
  CPP(ObjectPreventExtensions)                                                 \
  /* ES6 section 19.1.3.6 Object.prototype.toString () */                      \
  TFJ(ObjectProtoToString, 0)                                                  \
  CPP(ObjectPrototypePropertyIsEnumerable)                                     \
  CPP(ObjectPrototypeGetProto)                                                 \
  CPP(ObjectPrototypeSetProto)                                                 \
  CPP(ObjectSeal)                                                              \
  CPP(ObjectValues)                                                            \
                                                                               \
  TFS(HasProperty, BUILTIN, kNoExtraICState, HasProperty)                      \
  TFS(InstanceOf, BUILTIN, kNoExtraICState, Compare)                           \
  TFS(OrdinaryHasInstance, BUILTIN, kNoExtraICState, Compare)                  \
  TFS(ForInFilter, BUILTIN, kNoExtraICState, ForInFilter)                      \
                                                                               \
  /* Promise */                                                                \
  TFJ(PromiseGetCapabilitiesExecutor, 2)                                       \
  TFJ(NewPromiseCapability, 2)                                                 \
  TFJ(PromiseConstructor, 1)                                                   \
  TFJ(PromiseInternalConstructor, 1)                                           \
  TFJ(IsPromise, 1)                                                            \
  TFJ(PromiseResolveClosure, 1)                                                \
  TFJ(PromiseRejectClosure, 1)                                                 \
  TFJ(PromiseThen, 2)                                                          \
  TFJ(PromiseCatch, 1)                                                         \
  TFJ(PerformPromiseThen, 4)                                                   \
  TFJ(ResolvePromise, 2)                                                       \
  TFS(PromiseHandleReject, BUILTIN, kNoExtraICState, PromiseHandleReject)      \
  TFJ(PromiseHandle, 5)                                                        \
  TFJ(PromiseResolve, 1)                                                       \
  TFJ(PromiseReject, 1)                                                        \
  TFJ(InternalPromiseReject, 3)                                                \
                                                                               \
  /* Proxy */                                                                  \
  CPP(ProxyConstructor)                                                        \
  CPP(ProxyConstructor_ConstructStub)                                          \
                                                                               \
  /* Reflect */                                                                \
  ASM(ReflectApply)                                                            \
  ASM(ReflectConstruct)                                                        \
  CPP(ReflectDefineProperty)                                                   \
  CPP(ReflectDeleteProperty)                                                   \
  CPP(ReflectGet)                                                              \
  CPP(ReflectGetOwnPropertyDescriptor)                                         \
  CPP(ReflectGetPrototypeOf)                                                   \
  CPP(ReflectHas)                                                              \
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
  TFJ(RegExpConstructor, 2)                                                    \
  TFJ(RegExpInternalMatch, 2)                                                  \
  CPP(RegExpInputGetter)                                                       \
  CPP(RegExpInputSetter)                                                       \
  CPP(RegExpLastMatchGetter)                                                   \
  CPP(RegExpLastParenGetter)                                                   \
  CPP(RegExpLeftContextGetter)                                                 \
  TFJ(RegExpPrototypeCompile, 2)                                               \
  TFJ(RegExpPrototypeExec, 1)                                                  \
  TFJ(RegExpPrototypeFlagsGetter, 0)                                           \
  TFJ(RegExpPrototypeGlobalGetter, 0)                                          \
  TFJ(RegExpPrototypeIgnoreCaseGetter, 0)                                      \
  TFJ(RegExpPrototypeMatch, 1)                                                 \
  TFJ(RegExpPrototypeMultilineGetter, 0)                                       \
  TFJ(RegExpPrototypeReplace, 2)                                               \
  TFJ(RegExpPrototypeSearch, 1)                                                \
  TFJ(RegExpPrototypeSourceGetter, 0)                                          \
  TFJ(RegExpPrototypeSplit, 2)                                                 \
  TFJ(RegExpPrototypeStickyGetter, 0)                                          \
  TFJ(RegExpPrototypeTest, 1)                                                  \
  CPP(RegExpPrototypeToString)                                                 \
  TFJ(RegExpPrototypeUnicodeGetter, 0)                                         \
  CPP(RegExpRightContextGetter)                                                \
                                                                               \
  /* SharedArrayBuffer */                                                      \
  CPP(SharedArrayBufferPrototypeGetByteLength)                                 \
  TFJ(AtomicsLoad, 2)                                                          \
  TFJ(AtomicsStore, 3)                                                         \
                                                                               \
  /* String */                                                                 \
  ASM(StringConstructor)                                                       \
  ASM(StringConstructor_ConstructStub)                                         \
  CPP(StringFromCodePoint)                                                     \
  /* ES6 section 21.1.2.1 String.fromCharCode ( ...codeUnits ) */              \
  TFJ(StringFromCharCode, SharedFunctionInfo::kDontAdaptArgumentsSentinel)     \
  /* ES6 section 21.1.3.1 String.prototype.charAt ( pos ) */                   \
  TFJ(StringPrototypeCharAt, 1)                                                \
  /* ES6 section 21.1.3.2 String.prototype.charCodeAt ( pos ) */               \
  TFJ(StringPrototypeCharCodeAt, 1)                                            \
  /* ES6 section 21.1.3.6 */                                                   \
  /* String.prototype.endsWith ( searchString [ , endPosition ] ) */           \
  CPP(StringPrototypeEndsWith)                                                 \
  /* ES6 section 21.1.3.7 */                                                   \
  /* String.prototype.includes ( searchString [ , position ] ) */              \
  CPP(StringPrototypeIncludes)                                                 \
  /* ES6 section #sec-string.prototype.indexof */                              \
  /* String.prototype.indexOf ( searchString [ , position ] ) */               \
  TFJ(StringPrototypeIndexOf, SharedFunctionInfo::kDontAdaptArgumentsSentinel) \
  /* ES6 section 21.1.3.9 */                                                   \
  /* String.prototype.lastIndexOf ( searchString [ , position ] ) */           \
  CPP(StringPrototypeLastIndexOf)                                              \
  /* ES6 section 21.1.3.10 String.prototype.localeCompare ( that ) */          \
  CPP(StringPrototypeLocaleCompare)                                            \
  /* ES6 section 21.1.3.12 String.prototype.normalize ( [form] ) */            \
  CPP(StringPrototypeNormalize)                                                \
  /* ES6 section B.2.3.1 String.prototype.substr ( start, length ) */          \
  TFJ(StringPrototypeSubstr, 2)                                                \
  /* ES6 section 21.1.3.19 String.prototype.substring ( start, end ) */        \
  TFJ(StringPrototypeSubstring, 2)                                             \
  /* ES6 section 21.1.3.20 */                                                  \
  /* String.prototype.startsWith ( searchString [ , position ] ) */            \
  CPP(StringPrototypeStartsWith)                                               \
  /* ES6 section 21.1.3.25 String.prototype.toString () */                     \
  TFJ(StringPrototypeToString, 0)                                              \
  CPP(StringPrototypeTrim)                                                     \
  CPP(StringPrototypeTrimLeft)                                                 \
  CPP(StringPrototypeTrimRight)                                                \
  /* ES6 section 21.1.3.28 String.prototype.valueOf () */                      \
  TFJ(StringPrototypeValueOf, 0)                                               \
  /* ES6 #sec-string.prototype-@@iterator */                                   \
  TFJ(StringPrototypeIterator, 0)                                              \
                                                                               \
  /* StringIterator */                                                         \
  TFJ(StringIteratorPrototypeNext, 0)                                          \
                                                                               \
  /* Symbol */                                                                 \
  CPP(SymbolConstructor)                                                       \
  CPP(SymbolConstructor_ConstructStub)                                         \
  /* ES6 section 19.4.2.1 Symbol.for */                                        \
  CPP(SymbolFor)                                                               \
  /* ES6 section 19.4.2.5 Symbol.keyFor */                                     \
  CPP(SymbolKeyFor)                                                            \
  /* ES6 section 19.4.3.4 Symbol.prototype [ @@toPrimitive ] ( hint ) */       \
  TFJ(SymbolPrototypeToPrimitive, 1)                                           \
  /* ES6 section 19.4.3.2 Symbol.prototype.toString ( ) */                     \
  TFJ(SymbolPrototypeToString, 0)                                              \
  /* ES6 section 19.4.3.3 Symbol.prototype.valueOf ( ) */                      \
  TFJ(SymbolPrototypeValueOf, 0)                                               \
                                                                               \
  /* TypedArray */                                                             \
  CPP(TypedArrayPrototypeBuffer)                                               \
  /* ES6 section 22.2.3.2 get %TypedArray%.prototype.byteLength */             \
  TFJ(TypedArrayPrototypeByteLength, 0)                                        \
  /* ES6 section 22.2.3.3 get %TypedArray%.prototype.byteOffset */             \
  TFJ(TypedArrayPrototypeByteOffset, 0)                                        \
  /* ES6 section 22.2.3.18 get %TypedArray%.prototype.length */                \
  TFJ(TypedArrayPrototypeLength, 0)                                            \
  /* ES6 #sec-%typedarray%.prototype.entries */                                \
  TFJ(TypedArrayPrototypeEntries, 0)                                           \
  /* ES6 #sec-%typedarray%.prototype.keys */                                   \
  TFJ(TypedArrayPrototypeKeys, 0)                                              \
  /* ES6 #sec-%typedarray%.prototype.values */                                 \
  TFJ(TypedArrayPrototypeValues, 0)

#define IGNORE_BUILTIN(...)

#define BUILTIN_LIST_ALL(V) BUILTIN_LIST(V, V, V, V, V, V, V)

#define BUILTIN_LIST_C(V)                                            \
  BUILTIN_LIST(V, V, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN)

#define BUILTIN_LIST_A(V)                                                      \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               V, V, V)

#define BUILTIN_LIST_DBG(V)                                                    \
  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, IGNORE_BUILTIN, \
               IGNORE_BUILTIN, IGNORE_BUILTIN, V)

// Forward declarations.
class ObjectVisitor;
namespace compiler {
class CodeAssemblerState;
}

class Builtins {
 public:
  ~Builtins();

  // Generate all builtin code objects. Should be called once during
  // isolate initialization.
  void SetUp(Isolate* isolate, bool create_heap_objects);
  void TearDown();

  // Garbage collection support.
  void IterateBuiltins(ObjectVisitor* v);

  // Disassembler support.
  const char* Lookup(byte* pc);

  enum Name {
#define DEF_ENUM(Name, ...) k##Name,
    BUILTIN_LIST_ALL(DEF_ENUM)
#undef DEF_ENUM
        builtin_count
  };

#define DECLARE_BUILTIN_ACCESSOR(Name, ...) \
  V8_EXPORT_PRIVATE Handle<Code> Name();
  BUILTIN_LIST_ALL(DECLARE_BUILTIN_ACCESSOR)
#undef DECLARE_BUILTIN_ACCESSOR

  // Convenience wrappers.
  Handle<Code> CallFunction(
      ConvertReceiverMode = ConvertReceiverMode::kAny,
      TailCallMode tail_call_mode = TailCallMode::kDisallow);
  Handle<Code> Call(ConvertReceiverMode = ConvertReceiverMode::kAny,
                    TailCallMode tail_call_mode = TailCallMode::kDisallow);
  Handle<Code> CallBoundFunction(TailCallMode tail_call_mode);
  Handle<Code> NonPrimitiveToPrimitive(
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);
  Handle<Code> OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint);
  Handle<Code> InterpreterPushArgsAndCall(
      TailCallMode tail_call_mode,
      CallableType function_type = CallableType::kAny);
  Handle<Code> InterpreterPushArgsAndConstruct(CallableType function_type);
  Handle<Code> NewFunctionContext(ScopeType scope_type);
  Handle<Code> NewCloneShallowArray(AllocationSiteMode allocation_mode);
  Handle<Code> NewCloneShallowObject(int length);

  Code* builtin(Name name) {
    // Code::cast cannot be used here since we access builtins
    // during the marking phase of mark sweep. See IC::Clear.
    return reinterpret_cast<Code*>(builtins_[name]);
  }

  Address builtin_address(Name name) {
    return reinterpret_cast<Address>(&builtins_[name]);
  }

  static const char* name(int index);

  // Returns the C++ entry point for builtins implemented in C++, and the null
  // Address otherwise.
  static Address CppEntryOf(int index);

  static bool IsCpp(int index);
  static bool IsApi(int index);
  static bool HasCppImplementation(int index);

  bool is_initialized() const { return initialized_; }

  MUST_USE_RESULT static MaybeHandle<Object> InvokeApiFunction(
      Isolate* isolate, bool is_construct, Handle<HeapObject> function,
      Handle<Object> receiver, int argc, Handle<Object> args[],
      Handle<HeapObject> new_target);

  enum ExitFrameType { EXIT, BUILTIN_EXIT };

  static void Generate_Adaptor(MacroAssembler* masm, Address builtin_address,
                               ExitFrameType exit_frame_type);

  static bool AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                   Handle<JSObject> target_global_proxy);

 private:
  Builtins();

  static void Generate_CallFunction(MacroAssembler* masm,
                                    ConvertReceiverMode mode,
                                    TailCallMode tail_call_mode);

  static void Generate_CallBoundFunctionImpl(MacroAssembler* masm,
                                             TailCallMode tail_call_mode);

  static void Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode,
                            TailCallMode tail_call_mode);

  static void Generate_InterpreterPushArgsAndCallImpl(
      MacroAssembler* masm, TailCallMode tail_call_mode,
      CallableType function_type);

  static void Generate_InterpreterPushArgsAndConstructImpl(
      MacroAssembler* masm, CallableType function_type);

  enum class MathMaxMinKind { kMax, kMin };
  static void Generate_MathMaxMin(MacroAssembler* masm, MathMaxMinKind kind);

#define DECLARE_ASM(Name, ...) \
  static void Generate_##Name(MacroAssembler* masm);
#define DECLARE_TF(Name, ...) \
  static void Generate_##Name(compiler::CodeAssemblerState* state);

  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, DECLARE_TF, DECLARE_TF,
               DECLARE_ASM, DECLARE_ASM, DECLARE_ASM)

#undef DECLARE_ASM
#undef DECLARE_TF

  // Note: These are always Code objects, but to conform with
  // IterateBuiltins() above which assumes Object**'s for the callback
  // function f, we use an Object* array here.
  Object* builtins_[builtin_count];
  bool initialized_;

  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(Builtins);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_H_
