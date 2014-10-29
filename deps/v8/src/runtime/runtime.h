// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RUNTIME_H_
#define V8_RUNTIME_H_

#include "src/allocation.h"
#include "src/zone.h"

namespace v8 {
namespace internal {

// The interface to C++ runtime functions.

// ----------------------------------------------------------------------------
// RUNTIME_FUNCTION_LIST_ALWAYS defines runtime calls available in both
// release and debug mode.
// This macro should only be used by the macro RUNTIME_FUNCTION_LIST.

// WARNING: RUNTIME_FUNCTION_LIST_ALWAYS_* is a very large macro that caused
// MSVC Intellisense to crash.  It was broken into two macros to work around
// this problem. Please avoid large recursive macros whenever possible.
#define RUNTIME_FUNCTION_LIST_ALWAYS_1(F)                  \
  /* Property access */                                    \
  F(GetProperty, 2, 1)                                     \
  F(KeyedGetProperty, 2, 1)                                \
  F(DeleteProperty, 3, 1)                                  \
  F(HasOwnProperty, 2, 1)                                  \
  F(HasProperty, 2, 1)                                     \
  F(HasElement, 2, 1)                                      \
  F(IsPropertyEnumerable, 2, 1)                            \
  F(GetPropertyNames, 1, 1)                                \
  F(GetPropertyNamesFast, 1, 1)                            \
  F(GetOwnPropertyNames, 2, 1)                             \
  F(GetOwnElementNames, 1, 1)                              \
  F(GetInterceptorInfo, 1, 1)                              \
  F(GetNamedInterceptorPropertyNames, 1, 1)                \
  F(GetIndexedInterceptorElementNames, 1, 1)               \
  F(GetArgumentsProperty, 1, 1)                            \
  F(ToFastProperties, 1, 1)                                \
  F(FinishArrayPrototypeSetup, 1, 1)                       \
  F(SpecialArrayFunctions, 0, 1)                           \
  F(IsSloppyModeFunction, 1, 1)                            \
  F(GetDefaultReceiver, 1, 1)                              \
                                                           \
  F(GetPrototype, 1, 1)                                    \
  F(SetPrototype, 2, 1)                                    \
  F(InternalSetPrototype, 2, 1)                            \
  F(IsInPrototypeChain, 2, 1)                              \
                                                           \
  F(GetOwnProperty, 2, 1)                                  \
                                                           \
  F(IsExtensible, 1, 1)                                    \
  F(PreventExtensions, 1, 1)                               \
                                                           \
  /* Utilities */                                          \
  F(CheckIsBootstrapping, 0, 1)                            \
  F(GetRootNaN, 0, 1)                                      \
  F(Call, -1 /* >= 2 */, 1)                                \
  F(Apply, 5, 1)                                           \
  F(GetFunctionDelegate, 1, 1)                             \
  F(GetConstructorDelegate, 1, 1)                          \
  F(DeoptimizeFunction, 1, 1)                              \
  F(ClearFunctionTypeFeedback, 1, 1)                       \
  F(RunningInSimulator, 0, 1)                              \
  F(IsConcurrentRecompilationSupported, 0, 1)              \
  F(OptimizeFunctionOnNextCall, -1, 1)                     \
  F(NeverOptimizeFunction, 1, 1)                           \
  F(GetOptimizationStatus, -1, 1)                          \
  F(GetOptimizationCount, 1, 1)                            \
  F(UnblockConcurrentRecompilation, 0, 1)                  \
  F(CompileForOnStackReplacement, 1, 1)                    \
  F(SetAllocationTimeout, -1 /* 2 || 3 */, 1)              \
  F(SetNativeFlag, 1, 1)                                   \
  F(SetInlineBuiltinFlag, 1, 1)                            \
  F(StoreArrayLiteralElement, 5, 1)                        \
  F(DebugPrepareStepInIfStepping, 1, 1)                    \
  F(DebugPushPromise, 1, 1)                                \
  F(DebugPopPromise, 0, 1)                                 \
  F(DebugPromiseEvent, 1, 1)                               \
  F(DebugPromiseRejectEvent, 2, 1)                         \
  F(DebugAsyncTaskEvent, 1, 1)                             \
  F(FlattenString, 1, 1)                                   \
  F(LoadMutableDouble, 2, 1)                               \
  F(TryMigrateInstance, 1, 1)                              \
  F(NotifyContextDisposed, 0, 1)                           \
                                                           \
  /* Array join support */                                 \
  F(PushIfAbsent, 2, 1)                                    \
  F(ArrayConcat, 1, 1)                                     \
                                                           \
  /* Conversions */                                        \
  F(ToBool, 1, 1)                                          \
  F(Typeof, 1, 1)                                          \
                                                           \
  F(Booleanize, 2, 1) /* TODO(turbofan): Only temporary */ \
                                                           \
  F(StringToNumber, 1, 1)                                  \
  F(StringParseInt, 2, 1)                                  \
  F(StringParseFloat, 1, 1)                                \
  F(StringToLowerCase, 1, 1)                               \
  F(StringToUpperCase, 1, 1)                               \
  F(StringSplit, 3, 1)                                     \
  F(CharFromCode, 1, 1)                                    \
  F(URIEscape, 1, 1)                                       \
  F(URIUnescape, 1, 1)                                     \
                                                           \
  F(NumberToInteger, 1, 1)                                 \
  F(NumberToIntegerMapMinusZero, 1, 1)                     \
  F(NumberToJSUint32, 1, 1)                                \
  F(NumberToJSInt32, 1, 1)                                 \
                                                           \
  /* Arithmetic operations */                              \
  F(NumberAdd, 2, 1)                                       \
  F(NumberSub, 2, 1)                                       \
  F(NumberMul, 2, 1)                                       \
  F(NumberDiv, 2, 1)                                       \
  F(NumberMod, 2, 1)                                       \
  F(NumberUnaryMinus, 1, 1)                                \
  F(NumberImul, 2, 1)                                      \
                                                           \
  F(StringBuilderConcat, 3, 1)                             \
  F(StringBuilderJoin, 3, 1)                               \
  F(SparseJoinWithSeparator, 3, 1)                         \
                                                           \
  /* Bit operations */                                     \
  F(NumberOr, 2, 1)                                        \
  F(NumberAnd, 2, 1)                                       \
  F(NumberXor, 2, 1)                                       \
                                                           \
  F(NumberShl, 2, 1)                                       \
  F(NumberShr, 2, 1)                                       \
  F(NumberSar, 2, 1)                                       \
                                                           \
  /* Comparisons */                                        \
  F(NumberEquals, 2, 1)                                    \
  F(StringEquals, 2, 1)                                    \
                                                           \
  F(NumberCompare, 3, 1)                                   \
  F(SmiLexicographicCompare, 2, 1)                         \
                                                           \
  /* Math */                                               \
  F(MathAcos, 1, 1)                                        \
  F(MathAsin, 1, 1)                                        \
  F(MathAtan, 1, 1)                                        \
  F(MathFloorRT, 1, 1)                                     \
  F(MathAtan2, 2, 1)                                       \
  F(MathExpRT, 1, 1)                                       \
  F(RoundNumber, 1, 1)                                     \
  F(MathFround, 1, 1)                                      \
  F(RemPiO2, 1, 1)                                         \
                                                           \
  /* Regular expressions */                                \
  F(RegExpCompile, 3, 1)                                   \
  F(RegExpExecMultiple, 4, 1)                              \
  F(RegExpInitializeObject, 6, 1)                          \
                                                           \
  /* JSON */                                               \
  F(ParseJson, 1, 1)                                       \
  F(BasicJSONStringify, 1, 1)                              \
  F(QuoteJSONString, 1, 1)                                 \
                                                           \
  /* Strings */                                            \
  F(StringIndexOf, 3, 1)                                   \
  F(StringLastIndexOf, 3, 1)                               \
  F(StringLocaleCompare, 2, 1)                             \
  F(StringReplaceGlobalRegExpWithString, 4, 1)             \
  F(StringReplaceOneCharWithString, 3, 1)                  \
  F(StringMatch, 3, 1)                                     \
  F(StringTrim, 3, 1)                                      \
  F(StringToArray, 2, 1)                                   \
  F(NewStringWrapper, 1, 1)                                \
  F(NewString, 2, 1)                                       \
  F(TruncateString, 2, 1)                                  \
                                                           \
  /* Numbers */                                            \
  F(NumberToRadixString, 2, 1)                             \
  F(NumberToFixed, 2, 1)                                   \
  F(NumberToExponential, 2, 1)                             \
  F(NumberToPrecision, 2, 1)                               \
  F(IsValidSmi, 1, 1)                                      \
                                                           \
  /* Classes support */                                    \
  F(ToMethod, 2, 1)                                        \
  F(HomeObjectSymbol, 0, 1)                                \
  F(ThrowNonMethodError, 0, 1)                             \
  F(ThrowUnsupportedSuperError, 0, 1)                      \
  F(LoadFromSuper, 3, 1)                                   \
  F(StoreToSuper_Strict, 4, 1)                             \
  F(StoreToSuper_Sloppy, 4, 1)


#define RUNTIME_FUNCTION_LIST_ALWAYS_2(F)              \
  /* Reflection */                                     \
  F(FunctionSetInstanceClassName, 2, 1)                \
  F(FunctionSetLength, 2, 1)                           \
  F(FunctionSetPrototype, 2, 1)                        \
  F(FunctionGetName, 1, 1)                             \
  F(FunctionSetName, 2, 1)                             \
  F(FunctionNameShouldPrintAsAnonymous, 1, 1)          \
  F(FunctionMarkNameShouldPrintAsAnonymous, 1, 1)      \
  F(FunctionIsGenerator, 1, 1)                         \
  F(FunctionIsArrow, 1, 1)                             \
  F(FunctionIsConciseMethod, 1, 1)                     \
  F(FunctionBindArguments, 4, 1)                       \
  F(BoundFunctionGetBindings, 1, 1)                    \
  F(FunctionRemovePrototype, 1, 1)                     \
  F(FunctionGetSourceCode, 1, 1)                       \
  F(FunctionGetScript, 1, 1)                           \
  F(FunctionGetScriptSourcePosition, 1, 1)             \
  F(FunctionGetPositionForOffset, 2, 1)                \
  F(FunctionIsAPIFunction, 1, 1)                       \
  F(FunctionIsBuiltin, 1, 1)                           \
  F(GetScript, 1, 1)                                   \
  F(CollectStackTrace, 2, 1)                           \
  F(GetV8Version, 0, 1)                                \
  F(GeneratorGetFunction, 1, 1)                        \
  F(GeneratorGetContext, 1, 1)                         \
  F(GeneratorGetReceiver, 1, 1)                        \
  F(GeneratorGetContinuation, 1, 1)                    \
  F(GeneratorGetSourcePosition, 1, 1)                  \
                                                       \
  F(SetCode, 2, 1)                                     \
                                                       \
  F(CreateApiFunction, 2, 1)                           \
  F(IsTemplate, 1, 1)                                  \
  F(GetTemplateField, 2, 1)                            \
  F(DisableAccessChecks, 1, 1)                         \
  F(EnableAccessChecks, 1, 1)                          \
                                                       \
  /* Dates */                                          \
  F(DateCurrentTime, 0, 1)                             \
  F(DateParseString, 2, 1)                             \
  F(DateLocalTimezone, 1, 1)                           \
  F(DateToUTC, 1, 1)                                   \
  F(DateMakeDay, 2, 1)                                 \
  F(DateSetValue, 3, 1)                                \
  F(DateCacheVersion, 0, 1)                            \
                                                       \
  /* Globals */                                        \
  F(CompileString, 2, 1)                               \
                                                       \
  /* Eval */                                           \
  F(GlobalProxy, 1, 1)                                 \
  F(IsAttachedGlobal, 1, 1)                            \
                                                       \
  F(AddNamedProperty, 4, 1)                            \
  F(AddPropertyForTemplate, 4, 1)                      \
  F(SetProperty, 4, 1)                                 \
  F(AddElement, 4, 1)                                  \
  F(DefineApiAccessorProperty, 5, 1)                   \
  F(DefineDataPropertyUnchecked, 4, 1)                 \
  F(DefineAccessorPropertyUnchecked, 5, 1)             \
  F(GetDataProperty, 2, 1)                             \
  F(SetHiddenProperty, 3, 1)                           \
                                                       \
  /* Arrays */                                         \
  F(RemoveArrayHoles, 2, 1)                            \
  F(GetArrayKeys, 2, 1)                                \
  F(MoveArrayContents, 2, 1)                           \
  F(EstimateNumberOfElements, 1, 1)                    \
  F(NormalizeElements, 1, 1)                           \
                                                       \
  /* Getters and Setters */                            \
  F(LookupAccessor, 3, 1)                              \
                                                       \
  /* ES5 */                                            \
  F(ObjectFreeze, 1, 1)                                \
                                                       \
  /* Harmony modules */                                \
  F(IsJSModule, 1, 1)                                  \
                                                       \
  /* Harmony symbols */                                \
  F(CreateSymbol, 1, 1)                                \
  F(CreatePrivateSymbol, 1, 1)                         \
  F(CreateGlobalPrivateOwnSymbol, 1, 1)                \
  F(CreatePrivateOwnSymbol, 1, 1)                      \
  F(NewSymbolWrapper, 1, 1)                            \
  F(SymbolDescription, 1, 1)                           \
  F(SymbolRegistry, 0, 1)                              \
  F(SymbolIsPrivate, 1, 1)                             \
                                                       \
  /* Harmony proxies */                                \
  F(CreateJSProxy, 2, 1)                               \
  F(CreateJSFunctionProxy, 4, 1)                       \
  F(IsJSProxy, 1, 1)                                   \
  F(IsJSFunctionProxy, 1, 1)                           \
  F(GetHandler, 1, 1)                                  \
  F(GetCallTrap, 1, 1)                                 \
  F(GetConstructTrap, 1, 1)                            \
  F(Fix, 1, 1)                                         \
                                                       \
  /* Harmony sets */                                   \
  F(SetInitialize, 1, 1)                               \
  F(SetAdd, 2, 1)                                      \
  F(SetHas, 2, 1)                                      \
  F(SetDelete, 2, 1)                                   \
  F(SetClear, 1, 1)                                    \
  F(SetGetSize, 1, 1)                                  \
                                                       \
  F(SetIteratorInitialize, 3, 1)                       \
  F(SetIteratorNext, 2, 1)                             \
                                                       \
  /* Harmony maps */                                   \
  F(MapInitialize, 1, 1)                               \
  F(MapGet, 2, 1)                                      \
  F(MapHas, 2, 1)                                      \
  F(MapDelete, 2, 1)                                   \
  F(MapClear, 1, 1)                                    \
  F(MapSet, 3, 1)                                      \
  F(MapGetSize, 1, 1)                                  \
                                                       \
  F(MapIteratorInitialize, 3, 1)                       \
  F(MapIteratorNext, 2, 1)                             \
                                                       \
  /* Harmony weak maps and sets */                     \
  F(WeakCollectionInitialize, 1, 1)                    \
  F(WeakCollectionGet, 2, 1)                           \
  F(WeakCollectionHas, 2, 1)                           \
  F(WeakCollectionDelete, 2, 1)                        \
  F(WeakCollectionSet, 3, 1)                           \
                                                       \
  F(GetWeakMapEntries, 1, 1)                           \
  F(GetWeakSetValues, 1, 1)                            \
                                                       \
  /* Harmony events */                                 \
  F(EnqueueMicrotask, 1, 1)                            \
  F(RunMicrotasks, 0, 1)                               \
                                                       \
  /* Harmony observe */                                \
  F(IsObserved, 1, 1)                                  \
  F(SetIsObserved, 1, 1)                               \
  F(GetObservationState, 0, 1)                         \
  F(ObservationWeakMapCreate, 0, 1)                    \
  F(ObserverObjectAndRecordHaveSameOrigin, 3, 1)       \
  F(ObjectWasCreatedInCurrentOrigin, 1, 1)             \
  F(GetObjectContextObjectObserve, 1, 1)               \
  F(GetObjectContextObjectGetNotifier, 1, 1)           \
  F(GetObjectContextNotifierPerformChange, 1, 1)       \
                                                       \
  /* Harmony typed arrays */                           \
  F(ArrayBufferInitialize, 2, 1)                       \
  F(ArrayBufferSliceImpl, 3, 1)                        \
  F(ArrayBufferIsView, 1, 1)                           \
  F(ArrayBufferNeuter, 1, 1)                           \
                                                       \
  F(TypedArrayInitializeFromArrayLike, 4, 1)           \
  F(TypedArrayGetBuffer, 1, 1)                         \
  F(TypedArraySetFastCases, 3, 1)                      \
                                                       \
  F(DataViewGetBuffer, 1, 1)                           \
  F(DataViewGetInt8, 3, 1)                             \
  F(DataViewGetUint8, 3, 1)                            \
  F(DataViewGetInt16, 3, 1)                            \
  F(DataViewGetUint16, 3, 1)                           \
  F(DataViewGetInt32, 3, 1)                            \
  F(DataViewGetUint32, 3, 1)                           \
  F(DataViewGetFloat32, 3, 1)                          \
  F(DataViewGetFloat64, 3, 1)                          \
                                                       \
  F(DataViewSetInt8, 4, 1)                             \
  F(DataViewSetUint8, 4, 1)                            \
  F(DataViewSetInt16, 4, 1)                            \
  F(DataViewSetUint16, 4, 1)                           \
  F(DataViewSetInt32, 4, 1)                            \
  F(DataViewSetUint32, 4, 1)                           \
  F(DataViewSetFloat32, 4, 1)                          \
  F(DataViewSetFloat64, 4, 1)                          \
                                                       \
  /* Statements */                                     \
  F(NewObjectFromBound, 1, 1)                          \
                                                       \
  /* Declarations and initialization */                \
  F(InitializeVarGlobal, 3, 1)                         \
  F(OptimizeObjectForAddingMultipleProperties, 2, 1)   \
                                                       \
  /* Debugging */                                      \
  F(DebugPrint, 1, 1)                                  \
  F(GlobalPrint, 1, 1)                                 \
  F(DebugTrace, 0, 1)                                  \
  F(TraceEnter, 0, 1)                                  \
  F(TraceExit, 1, 1)                                   \
  F(Abort, 1, 1)                                       \
  F(AbortJS, 1, 1)                                     \
  /* ES5 */                                            \
  F(OwnKeys, 1, 1)                                     \
                                                       \
  /* Message objects */                                \
  F(MessageGetStartPosition, 1, 1)                     \
  F(MessageGetScript, 1, 1)                            \
                                                       \
  /* Pseudo functions - handled as macros by parser */ \
  F(IS_VAR, 1, 1)                                      \
                                                       \
  /* expose boolean functions from objects-inl.h */    \
  F(HasFastSmiElements, 1, 1)                          \
  F(HasFastSmiOrObjectElements, 1, 1)                  \
  F(HasFastObjectElements, 1, 1)                       \
  F(HasFastDoubleElements, 1, 1)                       \
  F(HasFastHoleyElements, 1, 1)                        \
  F(HasDictionaryElements, 1, 1)                       \
  F(HasSloppyArgumentsElements, 1, 1)                  \
  F(HasExternalUint8ClampedElements, 1, 1)             \
  F(HasExternalArrayElements, 1, 1)                    \
  F(HasExternalInt8Elements, 1, 1)                     \
  F(HasExternalUint8Elements, 1, 1)                    \
  F(HasExternalInt16Elements, 1, 1)                    \
  F(HasExternalUint16Elements, 1, 1)                   \
  F(HasExternalInt32Elements, 1, 1)                    \
  F(HasExternalUint32Elements, 1, 1)                   \
  F(HasExternalFloat32Elements, 1, 1)                  \
  F(HasExternalFloat64Elements, 1, 1)                  \
  F(HasFixedUint8ClampedElements, 1, 1)                \
  F(HasFixedInt8Elements, 1, 1)                        \
  F(HasFixedUint8Elements, 1, 1)                       \
  F(HasFixedInt16Elements, 1, 1)                       \
  F(HasFixedUint16Elements, 1, 1)                      \
  F(HasFixedInt32Elements, 1, 1)                       \
  F(HasFixedUint32Elements, 1, 1)                      \
  F(HasFixedFloat32Elements, 1, 1)                     \
  F(HasFixedFloat64Elements, 1, 1)                     \
  F(HasFastProperties, 1, 1)                           \
  F(TransitionElementsKind, 2, 1)                      \
  F(HaveSameMap, 2, 1)                                 \
  F(IsJSGlobalProxy, 1, 1)                             \
  F(ForInCacheArrayLength, 2, 1) /* TODO(turbofan): Only temporary */


#define RUNTIME_FUNCTION_LIST_ALWAYS_3(F)                    \
  /* String and Regexp */                                    \
  F(NumberToStringRT, 1, 1)                                  \
  F(RegExpConstructResult, 3, 1)                             \
  F(RegExpExecRT, 4, 1)                                      \
  F(StringAdd, 2, 1)                                         \
  F(SubString, 3, 1)                                         \
  F(InternalizeString, 1, 1)                                 \
  F(StringCompare, 2, 1)                                     \
  F(StringCharCodeAtRT, 2, 1)                                \
  F(GetFromCache, 2, 1)                                      \
                                                             \
  /* Compilation */                                          \
  F(CompileLazy, 1, 1)                                       \
  F(CompileOptimized, 2, 1)                                  \
  F(TryInstallOptimizedCode, 1, 1)                           \
  F(NotifyDeoptimized, 1, 1)                                 \
  F(NotifyStubFailure, 0, 1)                                 \
                                                             \
  /* Utilities */                                            \
  F(AllocateInNewSpace, 1, 1)                                \
  F(AllocateInTargetSpace, 2, 1)                             \
  F(AllocateHeapNumber, 0, 1)                                \
  F(NumberToSmi, 1, 1)                                       \
  F(NumberToStringSkipCache, 1, 1)                           \
                                                             \
  F(NewArguments, 1, 1) /* TODO(turbofan): Only temporary */ \
  F(NewSloppyArguments, 3, 1)                                \
  F(NewStrictArguments, 3, 1)                                \
                                                             \
  /* Harmony generators */                                   \
  F(CreateJSGeneratorObject, 0, 1)                           \
  F(SuspendJSGeneratorObject, 1, 1)                          \
  F(ResumeJSGeneratorObject, 3, 1)                           \
  F(ThrowGeneratorStateError, 1, 1)                          \
                                                             \
  /* Arrays */                                               \
  F(ArrayConstructor, -1, 1)                                 \
  F(InternalArrayConstructor, -1, 1)                         \
                                                             \
  /* Literals */                                             \
  F(MaterializeRegExpLiteral, 4, 1)                          \
  F(CreateObjectLiteral, 4, 1)                               \
  F(CreateArrayLiteral, 4, 1)                                \
  F(CreateArrayLiteralStubBailout, 3, 1)                     \
                                                             \
  /* Statements */                                           \
  F(NewClosure, 3, 1)                                        \
  F(NewClosureFromStubFailure, 1, 1)                         \
  F(NewObject, 1, 1)                                         \
  F(NewObjectWithAllocationSite, 2, 1)                       \
  F(FinalizeInstanceSize, 1, 1)                              \
  F(Throw, 1, 1)                                             \
  F(ReThrow, 1, 1)                                           \
  F(ThrowReferenceError, 1, 1)                               \
  F(ThrowNotDateError, 0, 1)                                 \
  F(StackGuard, 0, 1)                                        \
  F(Interrupt, 0, 1)                                         \
  F(PromoteScheduledException, 0, 1)                         \
                                                             \
  /* Contexts */                                             \
  F(NewGlobalContext, 2, 1)                                  \
  F(NewFunctionContext, 1, 1)                                \
  F(PushWithContext, 2, 1)                                   \
  F(PushCatchContext, 3, 1)                                  \
  F(PushBlockContext, 2, 1)                                  \
  F(PushModuleContext, 2, 1)                                 \
  F(DeleteLookupSlot, 2, 1)                                  \
  F(StoreLookupSlot, 4, 1)                                   \
                                                             \
  /* Declarations and initialization */                      \
  F(DeclareGlobals, 3, 1)                                    \
  F(DeclareModules, 1, 1)                                    \
  F(DeclareLookupSlot, 4, 1)                                 \
  F(InitializeConstGlobal, 2, 1)                             \
  F(InitializeLegacyConstLookupSlot, 3, 1)                   \
                                                             \
  /* Maths */                                                \
  F(MathPowSlow, 2, 1)                                       \
  F(MathPowRT, 2, 1)


#define RUNTIME_FUNCTION_LIST_RETURN_PAIR(F)              \
  F(LoadLookupSlot, 2, 2)                                 \
  F(LoadLookupSlotNoReferenceError, 2, 2)                 \
  F(ResolvePossiblyDirectEval, 5, 2)                      \
  F(ForInInit, 2, 2) /* TODO(turbofan): Only temporary */ \
  F(ForInNext, 4, 2) /* TODO(turbofan): Only temporary */


#define RUNTIME_FUNCTION_LIST_DEBUGGER(F)           \
  /* Debugger support*/                             \
  F(DebugBreak, 0, 1)                               \
  F(SetDebugEventListener, 2, 1)                    \
  F(Break, 0, 1)                                    \
  F(DebugGetPropertyDetails, 2, 1)                  \
  F(DebugGetProperty, 2, 1)                         \
  F(DebugPropertyTypeFromDetails, 1, 1)             \
  F(DebugPropertyAttributesFromDetails, 1, 1)       \
  F(DebugPropertyIndexFromDetails, 1, 1)            \
  F(DebugNamedInterceptorPropertyValue, 2, 1)       \
  F(DebugIndexedInterceptorElementValue, 2, 1)      \
  F(CheckExecutionState, 1, 1)                      \
  F(GetFrameCount, 1, 1)                            \
  F(GetFrameDetails, 2, 1)                          \
  F(GetScopeCount, 2, 1)                            \
  F(GetStepInPositions, 2, 1)                       \
  F(GetScopeDetails, 4, 1)                          \
  F(GetAllScopesDetails, 4, 1)                      \
  F(GetFunctionScopeCount, 1, 1)                    \
  F(GetFunctionScopeDetails, 2, 1)                  \
  F(SetScopeVariableValue, 6, 1)                    \
  F(DebugPrintScopes, 0, 1)                         \
  F(GetThreadCount, 1, 1)                           \
  F(GetThreadDetails, 2, 1)                         \
  F(SetDisableBreak, 1, 1)                          \
  F(GetBreakLocations, 2, 1)                        \
  F(SetFunctionBreakPoint, 3, 1)                    \
  F(SetScriptBreakPoint, 4, 1)                      \
  F(ClearBreakPoint, 1, 1)                          \
  F(ChangeBreakOnException, 2, 1)                   \
  F(IsBreakOnException, 1, 1)                       \
  F(PrepareStep, 4, 1)                              \
  F(ClearStepping, 0, 1)                            \
  F(DebugEvaluate, 6, 1)                            \
  F(DebugEvaluateGlobal, 4, 1)                      \
  F(DebugGetLoadedScripts, 0, 1)                    \
  F(DebugReferencedBy, 3, 1)                        \
  F(DebugConstructedBy, 2, 1)                       \
  F(DebugGetPrototype, 1, 1)                        \
  F(DebugSetScriptSource, 2, 1)                     \
  F(DebugCallbackSupportsStepping, 1, 1)            \
  F(SystemBreak, 0, 1)                              \
  F(DebugDisassembleFunction, 1, 1)                 \
  F(DebugDisassembleConstructor, 1, 1)              \
  F(FunctionGetInferredName, 1, 1)                  \
  F(LiveEditFindSharedFunctionInfosForScript, 1, 1) \
  F(LiveEditGatherCompileInfo, 2, 1)                \
  F(LiveEditReplaceScript, 3, 1)                    \
  F(LiveEditReplaceFunctionCode, 2, 1)              \
  F(LiveEditFunctionSourceUpdated, 1, 1)            \
  F(LiveEditFunctionSetScript, 2, 1)                \
  F(LiveEditReplaceRefToNestedFunction, 3, 1)       \
  F(LiveEditPatchFunctionPositions, 2, 1)           \
  F(LiveEditCheckAndDropActivations, 2, 1)          \
  F(LiveEditCompareStrings, 2, 1)                   \
  F(LiveEditRestartFrame, 2, 1)                     \
  F(GetFunctionCodePositionFromSource, 2, 1)        \
  F(ExecuteInDebugContext, 2, 1)                    \
                                                    \
  F(SetFlags, 1, 1)                                 \
  F(CollectGarbage, 1, 1)                           \
  F(GetHeapUsage, 0, 1)


#ifdef V8_I18N_SUPPORT
#define RUNTIME_FUNCTION_LIST_I18N_SUPPORT(F) \
  /* i18n support */                          \
  /* Standalone, helper methods. */           \
  F(CanonicalizeLanguageTag, 1, 1)            \
  F(AvailableLocalesOf, 1, 1)                 \
  F(GetDefaultICULocale, 0, 1)                \
  F(GetLanguageTagVariants, 1, 1)             \
  F(IsInitializedIntlObject, 1, 1)            \
  F(IsInitializedIntlObjectOfType, 2, 1)      \
  F(MarkAsInitializedIntlObjectOfType, 3, 1)  \
  F(GetImplFromInitializedIntlObject, 1, 1)   \
                                              \
  /* Date format and parse. */                \
  F(CreateDateTimeFormat, 3, 1)               \
  F(InternalDateFormat, 2, 1)                 \
  F(InternalDateParse, 2, 1)                  \
                                              \
  /* Number format and parse. */              \
  F(CreateNumberFormat, 3, 1)                 \
  F(InternalNumberFormat, 2, 1)               \
  F(InternalNumberParse, 2, 1)                \
                                              \
  /* Collator. */                             \
  F(CreateCollator, 3, 1)                     \
  F(InternalCompare, 3, 1)                    \
                                              \
  /* String.prototype.normalize. */           \
  F(StringNormalize, 2, 1)                    \
                                              \
  /* Break iterator. */                       \
  F(CreateBreakIterator, 3, 1)                \
  F(BreakIteratorAdoptText, 2, 1)             \
  F(BreakIteratorFirst, 1, 1)                 \
  F(BreakIteratorNext, 1, 1)                  \
  F(BreakIteratorCurrent, 1, 1)               \
  F(BreakIteratorBreakType, 1, 1)

#else
#define RUNTIME_FUNCTION_LIST_I18N_SUPPORT(F)
#endif


#ifdef DEBUG
#define RUNTIME_FUNCTION_LIST_DEBUG(F) \
  /* Testing */                        \
  F(ListNatives, 0, 1)
#else
#define RUNTIME_FUNCTION_LIST_DEBUG(F)
#endif

// ----------------------------------------------------------------------------
// RUNTIME_FUNCTION_LIST defines all runtime functions accessed
// either directly by id (via the code generator), or indirectly
// via a native call by name (from within JS code).
// Entries have the form F(name, number of arguments, number of return values).

#define RUNTIME_FUNCTION_LIST_RETURN_OBJECT(F) \
  RUNTIME_FUNCTION_LIST_ALWAYS_1(F)            \
  RUNTIME_FUNCTION_LIST_ALWAYS_2(F)            \
  RUNTIME_FUNCTION_LIST_ALWAYS_3(F)            \
  RUNTIME_FUNCTION_LIST_DEBUG(F)               \
  RUNTIME_FUNCTION_LIST_DEBUGGER(F)            \
  RUNTIME_FUNCTION_LIST_I18N_SUPPORT(F)


#define RUNTIME_FUNCTION_LIST(F)         \
  RUNTIME_FUNCTION_LIST_RETURN_OBJECT(F) \
  RUNTIME_FUNCTION_LIST_RETURN_PAIR(F)

// ----------------------------------------------------------------------------
// INLINE_FUNCTION_LIST defines all inlined functions accessed
// with a native call of the form %_name from within JS code.
// Entries have the form F(name, number of arguments, number of return values).
#define INLINE_FUNCTION_LIST(F)                             \
  F(IsSmi, 1, 1)                                            \
  F(IsNonNegativeSmi, 1, 1)                                 \
  F(IsArray, 1, 1)                                          \
  F(IsRegExp, 1, 1)                                         \
  F(IsConstructCall, 0, 1)                                  \
  F(CallFunction, -1 /* receiver + n args + function */, 1) \
  F(ArgumentsLength, 0, 1)                                  \
  F(Arguments, 1, 1)                                        \
  F(ValueOf, 1, 1)                                          \
  F(SetValueOf, 2, 1)                                       \
  F(DateField, 2 /* date object, field index */, 1)         \
  F(StringCharFromCode, 1, 1)                               \
  F(StringCharAt, 2, 1)                                     \
  F(OneByteSeqStringSetChar, 3, 1)                          \
  F(TwoByteSeqStringSetChar, 3, 1)                          \
  F(ObjectEquals, 2, 1)                                     \
  F(IsObject, 1, 1)                                         \
  F(IsFunction, 1, 1)                                       \
  F(IsUndetectableObject, 1, 1)                             \
  F(IsSpecObject, 1, 1)                                     \
  F(IsStringWrapperSafeForDefaultValueOf, 1, 1)             \
  F(MathPow, 2, 1)                                          \
  F(IsMinusZero, 1, 1)                                      \
  F(HasCachedArrayIndex, 1, 1)                              \
  F(GetCachedArrayIndex, 1, 1)                              \
  F(FastOneByteArrayJoin, 2, 1)                             \
  F(GeneratorNext, 2, 1)                                    \
  F(GeneratorThrow, 2, 1)                                   \
  F(DebugBreakInOptimizedCode, 0, 1)                        \
  F(ClassOf, 1, 1)                                          \
  F(StringCharCodeAt, 2, 1)                                 \
  F(StringAdd, 2, 1)                                        \
  F(SubString, 3, 1)                                        \
  F(StringCompare, 2, 1)                                    \
  F(RegExpExec, 4, 1)                                       \
  F(RegExpConstructResult, 3, 1)                            \
  F(GetFromCache, 2, 1)                                     \
  F(NumberToString, 1, 1)                                   \
  F(DebugIsActive, 0, 1)


// ----------------------------------------------------------------------------
// INLINE_OPTIMIZED_FUNCTION_LIST defines all inlined functions accessed
// with a native call of the form %_name from within JS code that also have
// a corresponding runtime function, that is called from non-optimized code.
// For the benefit of (fuzz) tests, the runtime version can also be called
// directly as %name (i.e. without the leading underscore).
// Entries have the form F(name, number of arguments, number of return values).
#define INLINE_OPTIMIZED_FUNCTION_LIST(F) \
  /* Typed Arrays */                      \
  F(TypedArrayInitialize, 5, 1)           \
  F(DataViewInitialize, 4, 1)             \
  F(MaxSmi, 0, 1)                         \
  F(TypedArrayMaxSizeInHeap, 0, 1)        \
  F(ArrayBufferViewGetByteLength, 1, 1)   \
  F(ArrayBufferViewGetByteOffset, 1, 1)   \
  F(TypedArrayGetLength, 1, 1)            \
  /* ArrayBuffer */                       \
  F(ArrayBufferGetByteLength, 1, 1)       \
  /* Maths */                             \
  F(ConstructDouble, 2, 1)                \
  F(DoubleHi, 1, 1)                       \
  F(DoubleLo, 1, 1)                       \
  F(MathSqrtRT, 1, 1)                     \
  F(MathLogRT, 1, 1)


//---------------------------------------------------------------------------
// Runtime provides access to all C++ runtime functions.

class RuntimeState {
 public:
  StaticResource<ConsStringIteratorOp>* string_iterator() {
    return &string_iterator_;
  }
  unibrow::Mapping<unibrow::ToUppercase, 128>* to_upper_mapping() {
    return &to_upper_mapping_;
  }
  unibrow::Mapping<unibrow::ToLowercase, 128>* to_lower_mapping() {
    return &to_lower_mapping_;
  }
  ConsStringIteratorOp* string_iterator_compare_x() {
    return &string_iterator_compare_x_;
  }
  ConsStringIteratorOp* string_iterator_compare_y() {
    return &string_iterator_compare_y_;
  }
  ConsStringIteratorOp* string_locale_compare_it1() {
    return &string_locale_compare_it1_;
  }
  ConsStringIteratorOp* string_locale_compare_it2() {
    return &string_locale_compare_it2_;
  }

 private:
  RuntimeState() {}
  // Non-reentrant string buffer for efficient general use in the runtime.
  StaticResource<ConsStringIteratorOp> string_iterator_;
  unibrow::Mapping<unibrow::ToUppercase, 128> to_upper_mapping_;
  unibrow::Mapping<unibrow::ToLowercase, 128> to_lower_mapping_;
  ConsStringIteratorOp string_iterator_compare_x_;
  ConsStringIteratorOp string_iterator_compare_y_;
  ConsStringIteratorOp string_locale_compare_it1_;
  ConsStringIteratorOp string_locale_compare_it2_;

  friend class Isolate;
  friend class Runtime;

  DISALLOW_COPY_AND_ASSIGN(RuntimeState);
};


class Runtime : public AllStatic {
 public:
  enum FunctionId {
#define F(name, nargs, ressize) k##name,
    RUNTIME_FUNCTION_LIST(F) INLINE_OPTIMIZED_FUNCTION_LIST(F)
#undef F
#define F(name, nargs, ressize) kInline##name,
    INLINE_FUNCTION_LIST(F)
#undef F
#define F(name, nargs, ressize) kInlineOptimized##name,
    INLINE_OPTIMIZED_FUNCTION_LIST(F)
#undef F
    kNumFunctions,
    kFirstInlineFunction = kInlineIsSmi
  };

  enum IntrinsicType { RUNTIME, INLINE, INLINE_OPTIMIZED };

  // Intrinsic function descriptor.
  struct Function {
    FunctionId function_id;
    IntrinsicType intrinsic_type;
    // The JS name of the function.
    const char* name;

    // The C++ (native) entry point.  NULL if the function is inlined.
    byte* entry;

    // The number of arguments expected. nargs is -1 if the function takes
    // a variable number of arguments.
    int nargs;
    // Size of result.  Most functions return a single pointer, size 1.
    int result_size;
  };

  static const int kNotFound = -1;

  // Add internalized strings for all the intrinsic function names to a
  // StringDictionary.
  static void InitializeIntrinsicFunctionNames(Isolate* isolate,
                                               Handle<NameDictionary> dict);

  // Get the intrinsic function with the given name, which must be internalized.
  static const Function* FunctionForName(Handle<String> name);

  // Get the intrinsic function with the given FunctionId.
  static const Function* FunctionForId(FunctionId id);

  // Get the intrinsic function with the given function entry address.
  static const Function* FunctionForEntry(Address ref);

  // General-purpose helper functions for runtime system.
  static int StringMatch(Isolate* isolate, Handle<String> sub,
                         Handle<String> pat, int index);

  // TODO(1240886): Some of the following methods are *not* handle safe, but
  // accept handle arguments. This seems fragile.

  // Support getting the characters in a string using [] notation as
  // in Firefox/SpiderMonkey, Safari and Opera.
  MUST_USE_RESULT static MaybeHandle<Object> GetElementOrCharAt(
      Isolate* isolate, Handle<Object> object, uint32_t index);

  MUST_USE_RESULT static MaybeHandle<Object> SetObjectProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key,
      Handle<Object> value, StrictMode strict_mode);

  MUST_USE_RESULT static MaybeHandle<Object> DefineObjectProperty(
      Handle<JSObject> object, Handle<Object> key, Handle<Object> value,
      PropertyAttributes attr);

  MUST_USE_RESULT static MaybeHandle<Object> DeleteObjectProperty(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> key,
      JSReceiver::DeleteMode mode);

  MUST_USE_RESULT static MaybeHandle<Object> HasObjectProperty(
      Isolate* isolate, Handle<JSReceiver> object, Handle<Object> key);

  MUST_USE_RESULT static MaybeHandle<Object> GetObjectProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key);

  static void SetupArrayBuffer(Isolate* isolate,
                               Handle<JSArrayBuffer> array_buffer,
                               bool is_external, void* data,
                               size_t allocated_length);

  static bool SetupArrayBufferAllocatingData(Isolate* isolate,
                                             Handle<JSArrayBuffer> array_buffer,
                                             size_t allocated_length,
                                             bool initialize = true);

  static void NeuterArrayBuffer(Handle<JSArrayBuffer> array_buffer);

  static void FreeArrayBuffer(Isolate* isolate,
                              JSArrayBuffer* phantom_array_buffer);

  enum TypedArrayId {
    // arrayIds below should be synchromized with typedarray.js natives.
    ARRAY_ID_UINT8 = 1,
    ARRAY_ID_INT8 = 2,
    ARRAY_ID_UINT16 = 3,
    ARRAY_ID_INT16 = 4,
    ARRAY_ID_UINT32 = 5,
    ARRAY_ID_INT32 = 6,
    ARRAY_ID_FLOAT32 = 7,
    ARRAY_ID_FLOAT64 = 8,
    ARRAY_ID_UINT8_CLAMPED = 9,
    ARRAY_ID_FIRST = ARRAY_ID_UINT8,
    ARRAY_ID_LAST = ARRAY_ID_UINT8_CLAMPED
  };

  static void ArrayIdToTypeAndSize(int array_id, ExternalArrayType* type,
                                   ElementsKind* external_elements_kind,
                                   ElementsKind* fixed_elements_kind,
                                   size_t* element_size);

  // Used in runtime.cc and hydrogen's VisitArrayLiteral.
  MUST_USE_RESULT static MaybeHandle<Object> CreateArrayLiteralBoilerplate(
      Isolate* isolate, Handle<FixedArray> literals,
      Handle<FixedArray> elements);
};


//---------------------------------------------------------------------------
// Constants used by interface to runtime functions.

class AllocateDoubleAlignFlag : public BitField<bool, 0, 1> {};
class AllocateTargetSpace : public BitField<AllocationSpace, 1, 3> {};

class DeclareGlobalsEvalFlag : public BitField<bool, 0, 1> {};
class DeclareGlobalsNativeFlag : public BitField<bool, 1, 1> {};
class DeclareGlobalsStrictMode : public BitField<StrictMode, 2, 1> {};
}
}  // namespace v8::internal

#endif  // V8_RUNTIME_H_
