// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RUNTIME_RUNTIME_H_
#define V8_RUNTIME_RUNTIME_H_

#include <memory>

#include "src/allocation.h"
#include "src/base/platform/time.h"
#include "src/globals.h"
#include "src/objects.h"
#include "src/unicode.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

// * Each intrinsic is consistently exposed in JavaScript via 2 names:
//    * %#name, which is always a runtime call.
//    * %_#name, which can be inlined or just a runtime call, the compiler in
//      question decides.
//
// * IntrinsicTypes are Runtime::RUNTIME and Runtime::INLINE, respectively.
//
// * IDs are Runtime::k##name and Runtime::kInline##name, respectively.
//
// * All intrinsics have a C++ implementation Runtime_##name.
//
// * Each compiler has an explicit list of intrisics it supports, falling back
//   to a simple runtime call if necessary.


// Entries have the form F(name, number of arguments, number of values):
// A variable number of arguments is specified by a -1, additional restrictions
// are specified by inline comments

#define FOR_EACH_INTRINSIC_ARRAY(F)  \
  F(FinishArrayPrototypeSetup, 1, 1) \
  F(SpecialArrayFunctions, 0, 1)     \
  F(TransitionElementsKind, 2, 1)    \
  F(RemoveArrayHoles, 2, 1)          \
  F(MoveArrayContents, 2, 1)         \
  F(EstimateNumberOfElements, 1, 1)  \
  F(GetArrayKeys, 2, 1)              \
  F(NewArray, -1 /* >= 3 */, 1)      \
  F(FunctionBind, -1, 1)             \
  F(NormalizeElements, 1, 1)         \
  F(GrowArrayElements, 2, 1)         \
  F(HasComplexElements, 1, 1)        \
  F(IsArray, 1, 1)                   \
  F(ArrayIsArray, 1, 1)              \
  F(FixedArrayGet, 2, 1)             \
  F(FixedArraySet, 3, 1)             \
  F(ArraySpeciesConstructor, 1, 1)   \
  F(ArrayIncludes_Slow, 3, 1)        \
  F(ArrayIndexOf, 3, 1)              \
  F(SpreadIterablePrepare, 1, 1)

#define FOR_EACH_INTRINSIC_ATOMICS(F)           \
  F(ThrowNotIntegerSharedTypedArrayError, 1, 1) \
  F(ThrowNotInt32SharedTypedArrayError, 1, 1)   \
  F(ThrowInvalidAtomicAccessIndexError, 0, 1)   \
  F(AtomicsCompareExchange, 4, 1)               \
  F(AtomicsAdd, 3, 1)                           \
  F(AtomicsSub, 3, 1)                           \
  F(AtomicsAnd, 3, 1)                           \
  F(AtomicsOr, 3, 1)                            \
  F(AtomicsXor, 3, 1)                           \
  F(AtomicsExchange, 3, 1)                      \
  F(AtomicsIsLockFree, 1, 1)                    \
  F(AtomicsWait, 4, 1)                          \
  F(AtomicsWake, 3, 1)                          \
  F(AtomicsNumWaitersForTesting, 2, 1)

#define FOR_EACH_INTRINSIC_CLASSES(F)        \
  F(ThrowUnsupportedSuperError, 0, 1)        \
  F(ThrowConstructorNonCallableError, 1, 1)  \
  F(ThrowStaticPrototypeError, 0, 1)         \
  F(ThrowSuperAlreadyCalledError, 0, 1)      \
  F(ThrowNotSuperConstructor, 2, 1)          \
  F(HomeObjectSymbol, 0, 1)                  \
  F(DefineClass, 4, 1)                       \
  F(InstallClassNameAccessor, 1, 1)          \
  F(InstallClassNameAccessorWithCheck, 1, 1) \
  F(LoadFromSuper, 3, 1)                     \
  F(LoadKeyedFromSuper, 3, 1)                \
  F(StoreToSuper_Strict, 4, 1)               \
  F(StoreToSuper_Sloppy, 4, 1)               \
  F(StoreKeyedToSuper_Strict, 4, 1)          \
  F(StoreKeyedToSuper_Sloppy, 4, 1)          \
  F(GetSuperConstructor, 1, 1)               \
  F(NewWithSpread, -1, 1)

#define FOR_EACH_INTRINSIC_COLLECTIONS(F) \
  F(StringGetRawHashField, 1, 1)          \
  F(TheHole, 0, 1)                        \
  F(JSCollectionGetTable, 1, 1)           \
  F(GenericHash, 1, 1)                    \
  F(SetInitialize, 1, 1)                  \
  F(SetGrow, 1, 1)                        \
  F(SetShrink, 1, 1)                      \
  F(SetClear, 1, 1)                       \
  F(SetIteratorInitialize, 3, 1)          \
  F(SetIteratorClone, 1, 1)               \
  F(SetIteratorNext, 2, 1)                \
  F(SetIteratorDetails, 1, 1)             \
  F(MapInitialize, 1, 1)                  \
  F(MapShrink, 1, 1)                      \
  F(MapClear, 1, 1)                       \
  F(MapGrow, 1, 1)                        \
  F(MapIteratorInitialize, 3, 1)          \
  F(MapIteratorClone, 1, 1)               \
  F(MapIteratorDetails, 1, 1)             \
  F(GetWeakMapEntries, 2, 1)              \
  F(MapIteratorNext, 2, 1)                \
  F(WeakCollectionInitialize, 1, 1)       \
  F(WeakCollectionGet, 3, 1)              \
  F(WeakCollectionHas, 3, 1)              \
  F(WeakCollectionDelete, 3, 1)           \
  F(WeakCollectionSet, 4, 1)              \
  F(GetWeakSetValues, 2, 1)

#define FOR_EACH_INTRINSIC_COMPILER(F)    \
  F(CompileLazy, 1, 1)                    \
  F(CompileBaseline, 1, 1)                \
  F(CompileOptimized_Concurrent, 1, 1)    \
  F(CompileOptimized_NotConcurrent, 1, 1) \
  F(NotifyStubFailure, 0, 1)              \
  F(NotifyDeoptimized, 1, 1)              \
  F(CompileForOnStackReplacement, 1, 1)   \
  F(TryInstallOptimizedCode, 1, 1)        \
  F(ResolvePossiblyDirectEval, 6, 1)      \
  F(InstantiateAsmJs, 4, 1)

#define FOR_EACH_INTRINSIC_DATE(F) \
  F(IsDate, 1, 1)                  \
  F(DateCurrentTime, 0, 1)         \
  F(ThrowNotDateError, 0, 1)

#define FOR_EACH_INTRINSIC_DEBUG(F)             \
  F(HandleDebuggerStatement, 0, 1)              \
  F(DebugBreak, 1, 1)                           \
  F(DebugBreakOnBytecode, 1, 1)                 \
  F(SetDebugEventListener, 2, 1)                \
  F(ScheduleBreak, 0, 1)                        \
  F(DebugGetInternalProperties, 1, 1)           \
  F(DebugGetPropertyDetails, 2, 1)              \
  F(DebugGetProperty, 2, 1)                     \
  F(DebugPropertyKindFromDetails, 1, 1)         \
  F(DebugPropertyAttributesFromDetails, 1, 1)   \
  F(CheckExecutionState, 1, 1)                  \
  F(GetFrameCount, 1, 1)                        \
  F(GetFrameDetails, 2, 1)                      \
  F(GetScopeCount, 2, 1)                        \
  F(GetScopeDetails, 4, 1)                      \
  F(GetAllScopesDetails, 4, 1)                  \
  F(GetFunctionScopeCount, 1, 1)                \
  F(GetFunctionScopeDetails, 2, 1)              \
  F(GetGeneratorScopeCount, 1, 1)               \
  F(GetGeneratorScopeDetails, 2, 1)             \
  F(SetScopeVariableValue, 6, 1)                \
  F(DebugPrintScopes, 0, 1)                     \
  F(SetBreakPointsActive, 1, 1)                 \
  F(GetBreakLocations, 2, 1)                    \
  F(SetFunctionBreakPoint, 3, 1)                \
  F(SetScriptBreakPoint, 4, 1)                  \
  F(ClearBreakPoint, 1, 1)                      \
  F(ChangeBreakOnException, 2, 1)               \
  F(IsBreakOnException, 1, 1)                   \
  F(PrepareStep, 2, 1)                          \
  F(PrepareStepFrame, 0, 1)                     \
  F(ClearStepping, 0, 1)                        \
  F(DebugEvaluate, 4, 1)                        \
  F(DebugEvaluateGlobal, 2, 1)                  \
  F(DebugGetLoadedScripts, 0, 1)                \
  F(DebugReferencedBy, 3, 1)                    \
  F(DebugConstructedBy, 2, 1)                   \
  F(DebugGetPrototype, 1, 1)                    \
  F(DebugSetScriptSource, 2, 1)                 \
  F(FunctionGetInferredName, 1, 1)              \
  F(FunctionGetDebugName, 1, 1)                 \
  F(ExecuteInDebugContext, 1, 1)                \
  F(GetDebugContext, 0, 1)                      \
  F(CollectGarbage, 1, 1)                       \
  F(GetHeapUsage, 0, 1)                         \
  F(GetScript, 1, 1)                            \
  F(ScriptLineCount, 1, 1)                      \
  F(ScriptLineStartPosition, 2, 1)              \
  F(ScriptLineEndPosition, 2, 1)                \
  F(ScriptLocationFromLine, 4, 1)               \
  F(ScriptLocationFromLine2, 4, 1)              \
  F(ScriptPositionInfo, 3, 1)                   \
  F(ScriptPositionInfo2, 3, 1)                  \
  F(ScriptSourceLine, 2, 1)                     \
  F(DebugOnFunctionCall, 1, 1)                  \
  F(DebugPrepareStepInSuspendedGenerator, 0, 1) \
  F(DebugRecordGenerator, 1, 1)                 \
  F(DebugPushPromise, 1, 1)                     \
  F(DebugPopPromise, 0, 1)                      \
  F(DebugPromiseReject, 2, 1)                   \
  F(DebugNextAsyncTaskId, 1, 1)                 \
  F(DebugAsyncEventEnqueueRecurring, 2, 1)      \
  F(DebugAsyncFunctionPromiseCreated, 1, 1)     \
  F(DebugIsActive, 0, 1)                        \
  F(DebugBreakInOptimizedCode, 0, 1)

#define FOR_EACH_INTRINSIC_ERROR(F) F(ErrorToString, 1, 1)

#define FOR_EACH_INTRINSIC_FORIN(F) \
  F(ForInEnumerate, 1, 1)           \
  F(ForInFilter, 2, 1)              \
  F(ForInHasProperty, 2, 1)         \
  F(ForInNext, 4, 1)

#define FOR_EACH_INTRINSIC_INTERPRETER(F) \
  F(InterpreterNewClosure, 4, 1)          \
  F(InterpreterTraceBytecodeEntry, 3, 1)  \
  F(InterpreterTraceBytecodeExit, 3, 1)   \
  F(InterpreterAdvanceBytecodeOffset, 2, 1)

#define FOR_EACH_INTRINSIC_FUNCTION(F)     \
  F(FunctionGetName, 1, 1)                 \
  F(FunctionSetName, 2, 1)                 \
  F(FunctionRemovePrototype, 1, 1)         \
  F(FunctionGetScript, 1, 1)               \
  F(FunctionGetScriptId, 1, 1)             \
  F(FunctionGetSourceCode, 1, 1)           \
  F(FunctionGetScriptSourcePosition, 1, 1) \
  F(FunctionGetContextData, 1, 1)          \
  F(FunctionSetInstanceClassName, 2, 1)    \
  F(FunctionSetLength, 2, 1)               \
  F(FunctionSetPrototype, 2, 1)            \
  F(FunctionIsAPIFunction, 1, 1)           \
  F(SetCode, 2, 1)                         \
  F(SetNativeFlag, 1, 1)                   \
  F(IsConstructor, 1, 1)                   \
  F(SetForceInlineFlag, 1, 1)              \
  F(Call, -1 /* >= 2 */, 1)                \
  F(ConvertReceiver, 1, 1)                 \
  F(IsFunction, 1, 1)                      \
  F(FunctionToString, 1, 1)

#define FOR_EACH_INTRINSIC_GENERATOR(F) \
  F(CreateJSGeneratorObject, 2, 1)      \
  F(GeneratorClose, 1, 1)               \
  F(GeneratorGetFunction, 1, 1)         \
  F(GeneratorGetReceiver, 1, 1)         \
  F(GeneratorGetContext, 1, 1)          \
  F(GeneratorGetInputOrDebugPos, 1, 1)  \
  F(GeneratorGetContinuation, 1, 1)     \
  F(GeneratorGetSourcePosition, 1, 1)   \
  F(GeneratorGetResumeMode, 1, 1)

#ifdef V8_I18N_SUPPORT
#define FOR_EACH_INTRINSIC_I18N(F)           \
  F(CanonicalizeLanguageTag, 1, 1)           \
  F(AvailableLocalesOf, 1, 1)                \
  F(GetDefaultICULocale, 0, 1)               \
  F(GetLanguageTagVariants, 1, 1)            \
  F(IsInitializedIntlObject, 1, 1)           \
  F(IsInitializedIntlObjectOfType, 2, 1)     \
  F(MarkAsInitializedIntlObjectOfType, 2, 1) \
  F(CreateDateTimeFormat, 3, 1)              \
  F(InternalDateFormat, 2, 1)                \
  F(InternalDateFormatToParts, 2, 1)         \
  F(CreateNumberFormat, 3, 1)                \
  F(InternalNumberFormat, 2, 1)              \
  F(CreateCollator, 3, 1)                    \
  F(InternalCompare, 3, 1)                   \
  F(StringNormalize, 2, 1)                   \
  F(CreateBreakIterator, 3, 1)               \
  F(BreakIteratorAdoptText, 2, 1)            \
  F(BreakIteratorFirst, 1, 1)                \
  F(BreakIteratorNext, 1, 1)                 \
  F(BreakIteratorCurrent, 1, 1)              \
  F(BreakIteratorBreakType, 1, 1)            \
  F(StringToLowerCaseI18N, 1, 1)             \
  F(StringToUpperCaseI18N, 1, 1)             \
  F(StringLocaleConvertCase, 3, 1)           \
  F(DateCacheVersion, 0, 1)
#else
#define FOR_EACH_INTRINSIC_I18N(F)
#endif

#define FOR_EACH_INTRINSIC_INTERNAL(F)              \
  F(AllocateInNewSpace, 1, 1)                       \
  F(AllocateInTargetSpace, 2, 1)                    \
  F(AllocateSeqOneByteString, 1, 1)                 \
  F(AllocateSeqTwoByteString, 1, 1)                 \
  F(CheckIsBootstrapping, 0, 1)                     \
  F(CreateListFromArrayLike, 1, 1)                  \
  F(EnqueueMicrotask, 1, 1)                         \
  F(EnqueuePromiseReactionJob, 3, 1)                \
  F(EnqueuePromiseResolveThenableJob, 1, 1)         \
  F(GetAndResetRuntimeCallStats, -1 /* <= 2 */, 1)  \
  F(ExportExperimentalFromRuntime, 1, 1)            \
  F(ExportFromRuntime, 1, 1)                        \
  F(IncrementUseCounter, 1, 1)                      \
  F(InstallToContext, 1, 1)                         \
  F(Interrupt, 0, 1)                                \
  F(IS_VAR, 1, 1)                                   \
  F(NewReferenceError, 2, 1)                        \
  F(NewSyntaxError, 2, 1)                           \
  F(NewTypeError, 2, 1)                             \
  F(OrdinaryHasInstance, 2, 1)                      \
  F(ReportPromiseReject, 2, 1)                      \
  F(PromiseHookInit, 2, 1)                          \
  F(PromiseHookResolve, 1, 1)                       \
  F(PromiseHookBefore, 1, 1)                        \
  F(PromiseHookAfter, 1, 1)                         \
  F(PromiseMarkAsHandled, 1, 1)                     \
  F(PromiseMarkHandledHint, 1, 1)                   \
  F(PromiseRejectEventFromStack, 2, 1)              \
  F(PromiseRevokeReject, 1, 1)                      \
  F(PromiseResult, 1, 1)                            \
  F(PromiseStatus, 1, 1)                            \
  F(PromoteScheduledException, 0, 1)                \
  F(ReThrow, 1, 1)                                  \
  F(RunMicrotasks, 0, 1)                            \
  F(StackGuard, 0, 1)                               \
  F(Throw, 1, 1)                                    \
  F(ThrowApplyNonFunction, 1, 1)                    \
  F(ThrowCannotConvertToPrimitive, 0, 1)            \
  F(ThrowCalledNonCallable, 1, 1)                   \
  F(ThrowCalledOnNullOrUndefined, 1, 1)             \
  F(ThrowConstructedNonConstructable, 1, 1)         \
  F(ThrowDerivedConstructorReturnedNonObject, 0, 1) \
  F(ThrowGeneratorRunning, 0, 1)                    \
  F(ThrowIllegalInvocation, 0, 1)                   \
  F(ThrowIncompatibleMethodReceiver, 2, 1)          \
  F(ThrowInvalidHint, 1, 1)                         \
  F(ThrowInvalidStringLength, 0, 1)                 \
  F(ThrowIteratorResultNotAnObject, 1, 1)           \
  F(ThrowSymbolIteratorInvalid, 0, 1)               \
  F(ThrowNotGeneric, 1, 1)                          \
  F(ThrowReferenceError, 1, 1)                      \
  F(ThrowStackOverflow, 0, 1)                       \
  F(ThrowTypeError, -1 /* >= 1 */, 1)               \
  F(ThrowUndefinedOrNullToObject, 1, 1)             \
  F(Typeof, 1, 1)                                   \
  F(UnwindAndFindExceptionHandler, 0, 1)            \
  F(AllowDynamicFunction, 1, 1)

#define FOR_EACH_INTRINSIC_LITERALS(F) \
  F(CreateRegExpLiteral, 4, 1)         \
  F(CreateObjectLiteral, 4, 1)         \
  F(CreateArrayLiteral, 4, 1)          \
  F(CreateArrayLiteralStubBailout, 3, 1)

#define FOR_EACH_INTRINSIC_LIVEEDIT(F)              \
  F(LiveEditFindSharedFunctionInfosForScript, 1, 1) \
  F(LiveEditGatherCompileInfo, 2, 1)                \
  F(LiveEditReplaceScript, 3, 1)                    \
  F(LiveEditFunctionSourceUpdated, 2, 1)            \
  F(LiveEditReplaceFunctionCode, 2, 1)              \
  F(LiveEditFixupScript, 2, 1)                      \
  F(LiveEditFunctionSetScript, 2, 1)                \
  F(LiveEditReplaceRefToNestedFunction, 3, 1)       \
  F(LiveEditPatchFunctionPositions, 2, 1)           \
  F(LiveEditCheckAndDropActivations, 3, 1)          \
  F(LiveEditCompareStrings, 2, 1)                   \
  F(LiveEditRestartFrame, 2, 1)

#define FOR_EACH_INTRINSIC_MATHS(F) F(GenerateRandomNumbers, 0, 1)

#define FOR_EACH_INTRINSIC_MODULE(F) \
  F(GetModuleNamespace, 1, 1)        \
  F(LoadModuleVariable, 1, 1)        \
  F(StoreModuleVariable, 2, 1)

#define FOR_EACH_INTRINSIC_NUMBERS(F)  \
  F(IsValidSmi, 1, 1)                  \
  F(StringToNumber, 1, 1)              \
  F(StringParseInt, 2, 1)              \
  F(StringParseFloat, 1, 1)            \
  F(NumberToString, 1, 1)              \
  F(NumberToStringSkipCache, 1, 1)     \
  F(NumberToSmi, 1, 1)                 \
  F(SmiLexicographicCompare, 2, 1)     \
  F(MaxSmi, 0, 1)                      \
  F(IsSmi, 1, 1)                       \
  F(GetRootNaN, 0, 1)                  \
  F(GetHoleNaNUpper, 0, 1)             \
  F(GetHoleNaNLower, 0, 1)

#define FOR_EACH_INTRINSIC_OBJECT(F)                 \
  F(GetPrototype, 1, 1)                              \
  F(ObjectHasOwnProperty, 2, 1)                      \
  F(ObjectCreate, 2, 1)                              \
  F(InternalSetPrototype, 2, 1)                      \
  F(OptimizeObjectForAddingMultipleProperties, 2, 1) \
  F(GetProperty, 2, 1)                               \
  F(KeyedGetProperty, 2, 1)                          \
  F(AddNamedProperty, 4, 1)                          \
  F(SetProperty, 4, 1)                               \
  F(AddElement, 3, 1)                                \
  F(AppendElement, 2, 1)                             \
  F(DeleteProperty_Sloppy, 2, 1)                     \
  F(DeleteProperty_Strict, 2, 1)                     \
  F(HasProperty, 2, 1)                               \
  F(GetOwnPropertyKeys, 2, 1)                        \
  F(GetInterceptorInfo, 1, 1)                        \
  F(ToFastProperties, 1, 1)                          \
  F(AllocateHeapNumber, 0, 1)                        \
  F(NewObject, 2, 1)                                 \
  F(FinalizeInstanceSize, 1, 1)                      \
  F(LoadMutableDouble, 2, 1)                         \
  F(TryMigrateInstance, 1, 1)                        \
  F(IsJSGlobalProxy, 1, 1)                           \
  F(DefineAccessorPropertyUnchecked, 5, 1)           \
  F(DefineDataPropertyInLiteral, 6, 1)               \
  F(GetDataProperty, 2, 1)                           \
  F(GetConstructorName, 1, 1)                        \
  F(HasFastPackedElements, 1, 1)                     \
  F(ValueOf, 1, 1)                                   \
  F(IsJSReceiver, 1, 1)                              \
  F(ClassOf, 1, 1)                                   \
  F(CopyDataProperties, 2, 1)                        \
  F(DefineGetterPropertyUnchecked, 4, 1)             \
  F(DefineSetterPropertyUnchecked, 4, 1)             \
  F(ToObject, 1, 1)                                  \
  F(ToPrimitive, 1, 1)                               \
  F(ToPrimitive_Number, 1, 1)                        \
  F(ToNumber, 1, 1)                                  \
  F(ToInteger, 1, 1)                                 \
  F(ToLength, 1, 1)                                  \
  F(ToString, 1, 1)                                  \
  F(ToName, 1, 1)                                    \
  F(SameValue, 2, 1)                                 \
  F(SameValueZero, 2, 1)                             \
  F(Compare, 3, 1)                                   \
  F(HasInPrototypeChain, 2, 1)                       \
  F(CreateIterResultObject, 2, 1)                    \
  F(CreateKeyValueArray, 2, 1)                       \
  F(IsAccessCheckNeeded, 1, 1)                       \
  F(CreateDataProperty, 3, 1)

#define FOR_EACH_INTRINSIC_OPERATORS(F) \
  F(Multiply, 2, 1)                     \
  F(Divide, 2, 1)                       \
  F(Modulus, 2, 1)                      \
  F(Add, 2, 1)                          \
  F(Subtract, 2, 1)                     \
  F(ShiftLeft, 2, 1)                    \
  F(ShiftRight, 2, 1)                   \
  F(ShiftRightLogical, 2, 1)            \
  F(BitwiseAnd, 2, 1)                   \
  F(BitwiseOr, 2, 1)                    \
  F(BitwiseXor, 2, 1)                   \
  F(Equal, 2, 1)                        \
  F(NotEqual, 2, 1)                     \
  F(StrictEqual, 2, 1)                  \
  F(StrictNotEqual, 2, 1)               \
  F(LessThan, 2, 1)                     \
  F(GreaterThan, 2, 1)                  \
  F(LessThanOrEqual, 2, 1)              \
  F(GreaterThanOrEqual, 2, 1)           \
  F(InstanceOf, 2, 1)

#define FOR_EACH_INTRINSIC_PROXY(F)     \
  F(IsJSProxy, 1, 1)                    \
  F(JSProxyCall, -1 /* >= 2 */, 1)      \
  F(JSProxyConstruct, -1 /* >= 3 */, 1) \
  F(JSProxyGetTarget, 1, 1)             \
  F(JSProxyGetHandler, 1, 1)            \
  F(JSProxyRevoke, 1, 1)

#define FOR_EACH_INTRINSIC_REGEXP(F)                \
  F(IsRegExp, 1, 1)                                 \
  F(RegExpCreate, 1, 1)                             \
  F(RegExpExec, 4, 1)                               \
  F(RegExpExecMultiple, 4, 1)                       \
  F(RegExpExecReThrow, 4, 1)                        \
  F(RegExpInitializeAndCompile, 3, 1)               \
  F(RegExpInternalReplace, 3, 1)                    \
  F(RegExpReplace, 3, 1)                            \
  F(RegExpSplit, 3, 1)                              \
  F(StringReplaceGlobalRegExpWithString, 4, 1)      \
  F(StringReplaceNonGlobalRegExpWithFunction, 3, 1) \
  F(StringSplit, 3, 1)

#define FOR_EACH_INTRINSIC_SCOPES(F)    \
  F(ThrowConstAssignError, 0, 1)        \
  F(DeclareGlobals, 3, 1)               \
  F(DeclareGlobalsForInterpreter, 3, 1) \
  F(InitializeVarGlobal, 3, 1)          \
  F(DeclareEvalFunction, 2, 1)          \
  F(DeclareEvalVar, 1, 1)               \
  F(NewSloppyArguments_Generic, 1, 1)   \
  F(NewStrictArguments, 1, 1)           \
  F(NewRestParameter, 1, 1)             \
  F(NewSloppyArguments, 3, 1)           \
  F(NewArgumentsElements, 2, 1)         \
  F(NewClosure, 3, 1)                   \
  F(NewClosure_Tenured, 3, 1)           \
  F(NewScriptContext, 2, 1)             \
  F(NewFunctionContext, 2, 1)           \
  F(PushModuleContext, 3, 1)            \
  F(PushWithContext, 3, 1)              \
  F(PushCatchContext, 4, 1)             \
  F(PushBlockContext, 2, 1)             \
  F(DeleteLookupSlot, 1, 1)             \
  F(LoadLookupSlot, 1, 1)               \
  F(LoadLookupSlotInsideTypeof, 1, 1)   \
  F(StoreLookupSlot_Sloppy, 2, 1)       \
  F(StoreLookupSlot_Strict, 2, 1)

#define FOR_EACH_INTRINSIC_SIMD(F)     \
  F(IsSimdValue, 1, 1)                 \
  F(CreateFloat32x4, 4, 1)             \
  F(CreateInt32x4, 4, 1)               \
  F(CreateUint32x4, 4, 1)              \
  F(CreateBool32x4, 4, 1)              \
  F(CreateInt16x8, 8, 1)               \
  F(CreateUint16x8, 8, 1)              \
  F(CreateBool16x8, 8, 1)              \
  F(CreateInt8x16, 16, 1)              \
  F(CreateUint8x16, 16, 1)             \
  F(CreateBool8x16, 16, 1)             \
  F(Float32x4Check, 1, 1)              \
  F(Float32x4ExtractLane, 2, 1)        \
  F(Float32x4ReplaceLane, 3, 1)        \
  F(Float32x4Abs, 1, 1)                \
  F(Float32x4Neg, 1, 1)                \
  F(Float32x4Sqrt, 1, 1)               \
  F(Float32x4RecipApprox, 1, 1)        \
  F(Float32x4RecipSqrtApprox, 1, 1)    \
  F(Float32x4Add, 2, 1)                \
  F(Float32x4Sub, 2, 1)                \
  F(Float32x4Mul, 2, 1)                \
  F(Float32x4Div, 2, 1)                \
  F(Float32x4Min, 2, 1)                \
  F(Float32x4Max, 2, 1)                \
  F(Float32x4MinNum, 2, 1)             \
  F(Float32x4MaxNum, 2, 1)             \
  F(Float32x4Equal, 2, 1)              \
  F(Float32x4NotEqual, 2, 1)           \
  F(Float32x4LessThan, 2, 1)           \
  F(Float32x4LessThanOrEqual, 2, 1)    \
  F(Float32x4GreaterThan, 2, 1)        \
  F(Float32x4GreaterThanOrEqual, 2, 1) \
  F(Float32x4Select, 3, 1)             \
  F(Float32x4Swizzle, 5, 1)            \
  F(Float32x4Shuffle, 6, 1)            \
  F(Float32x4FromInt32x4, 1, 1)        \
  F(Float32x4FromUint32x4, 1, 1)       \
  F(Float32x4FromInt32x4Bits, 1, 1)    \
  F(Float32x4FromUint32x4Bits, 1, 1)   \
  F(Float32x4FromInt16x8Bits, 1, 1)    \
  F(Float32x4FromUint16x8Bits, 1, 1)   \
  F(Float32x4FromInt8x16Bits, 1, 1)    \
  F(Float32x4FromUint8x16Bits, 1, 1)   \
  F(Float32x4Load, 2, 1)               \
  F(Float32x4Load1, 2, 1)              \
  F(Float32x4Load2, 2, 1)              \
  F(Float32x4Load3, 2, 1)              \
  F(Float32x4Store, 3, 1)              \
  F(Float32x4Store1, 3, 1)             \
  F(Float32x4Store2, 3, 1)             \
  F(Float32x4Store3, 3, 1)             \
  F(Int32x4Check, 1, 1)                \
  F(Int32x4ExtractLane, 2, 1)          \
  F(Int32x4ReplaceLane, 3, 1)          \
  F(Int32x4Neg, 1, 1)                  \
  F(Int32x4Add, 2, 1)                  \
  F(Int32x4Sub, 2, 1)                  \
  F(Int32x4Mul, 2, 1)                  \
  F(Int32x4Min, 2, 1)                  \
  F(Int32x4Max, 2, 1)                  \
  F(Int32x4And, 2, 1)                  \
  F(Int32x4Or, 2, 1)                   \
  F(Int32x4Xor, 2, 1)                  \
  F(Int32x4Not, 1, 1)                  \
  F(Int32x4ShiftLeftByScalar, 2, 1)    \
  F(Int32x4ShiftRightByScalar, 2, 1)   \
  F(Int32x4Equal, 2, 1)                \
  F(Int32x4NotEqual, 2, 1)             \
  F(Int32x4LessThan, 2, 1)             \
  F(Int32x4LessThanOrEqual, 2, 1)      \
  F(Int32x4GreaterThan, 2, 1)          \
  F(Int32x4GreaterThanOrEqual, 2, 1)   \
  F(Int32x4Select, 3, 1)               \
  F(Int32x4Swizzle, 5, 1)              \
  F(Int32x4Shuffle, 6, 1)              \
  F(Int32x4FromFloat32x4, 1, 1)        \
  F(Int32x4FromUint32x4, 1, 1)         \
  F(Int32x4FromFloat32x4Bits, 1, 1)    \
  F(Int32x4FromUint32x4Bits, 1, 1)     \
  F(Int32x4FromInt16x8Bits, 1, 1)      \
  F(Int32x4FromUint16x8Bits, 1, 1)     \
  F(Int32x4FromInt8x16Bits, 1, 1)      \
  F(Int32x4FromUint8x16Bits, 1, 1)     \
  F(Int32x4Load, 2, 1)                 \
  F(Int32x4Load1, 2, 1)                \
  F(Int32x4Load2, 2, 1)                \
  F(Int32x4Load3, 2, 1)                \
  F(Int32x4Store, 3, 1)                \
  F(Int32x4Store1, 3, 1)               \
  F(Int32x4Store2, 3, 1)               \
  F(Int32x4Store3, 3, 1)               \
  F(Uint32x4Check, 1, 1)               \
  F(Uint32x4ExtractLane, 2, 1)         \
  F(Uint32x4ReplaceLane, 3, 1)         \
  F(Uint32x4Add, 2, 1)                 \
  F(Uint32x4Sub, 2, 1)                 \
  F(Uint32x4Mul, 2, 1)                 \
  F(Uint32x4Min, 2, 1)                 \
  F(Uint32x4Max, 2, 1)                 \
  F(Uint32x4And, 2, 1)                 \
  F(Uint32x4Or, 2, 1)                  \
  F(Uint32x4Xor, 2, 1)                 \
  F(Uint32x4Not, 1, 1)                 \
  F(Uint32x4ShiftLeftByScalar, 2, 1)   \
  F(Uint32x4ShiftRightByScalar, 2, 1)  \
  F(Uint32x4Equal, 2, 1)               \
  F(Uint32x4NotEqual, 2, 1)            \
  F(Uint32x4LessThan, 2, 1)            \
  F(Uint32x4LessThanOrEqual, 2, 1)     \
  F(Uint32x4GreaterThan, 2, 1)         \
  F(Uint32x4GreaterThanOrEqual, 2, 1)  \
  F(Uint32x4Select, 3, 1)              \
  F(Uint32x4Swizzle, 5, 1)             \
  F(Uint32x4Shuffle, 6, 1)             \
  F(Uint32x4FromFloat32x4, 1, 1)       \
  F(Uint32x4FromInt32x4, 1, 1)         \
  F(Uint32x4FromFloat32x4Bits, 1, 1)   \
  F(Uint32x4FromInt32x4Bits, 1, 1)     \
  F(Uint32x4FromInt16x8Bits, 1, 1)     \
  F(Uint32x4FromUint16x8Bits, 1, 1)    \
  F(Uint32x4FromInt8x16Bits, 1, 1)     \
  F(Uint32x4FromUint8x16Bits, 1, 1)    \
  F(Uint32x4Load, 2, 1)                \
  F(Uint32x4Load1, 2, 1)               \
  F(Uint32x4Load2, 2, 1)               \
  F(Uint32x4Load3, 2, 1)               \
  F(Uint32x4Store, 3, 1)               \
  F(Uint32x4Store1, 3, 1)              \
  F(Uint32x4Store2, 3, 1)              \
  F(Uint32x4Store3, 3, 1)              \
  F(Bool32x4Check, 1, 1)               \
  F(Bool32x4ExtractLane, 2, 1)         \
  F(Bool32x4ReplaceLane, 3, 1)         \
  F(Bool32x4And, 2, 1)                 \
  F(Bool32x4Or, 2, 1)                  \
  F(Bool32x4Xor, 2, 1)                 \
  F(Bool32x4Not, 1, 1)                 \
  F(Bool32x4AnyTrue, 1, 1)             \
  F(Bool32x4AllTrue, 1, 1)             \
  F(Bool32x4Swizzle, 5, 1)             \
  F(Bool32x4Shuffle, 6, 1)             \
  F(Bool32x4Equal, 2, 1)               \
  F(Bool32x4NotEqual, 2, 1)            \
  F(Int16x8Check, 1, 1)                \
  F(Int16x8ExtractLane, 2, 1)          \
  F(Int16x8ReplaceLane, 3, 1)          \
  F(Int16x8Neg, 1, 1)                  \
  F(Int16x8Add, 2, 1)                  \
  F(Int16x8AddSaturate, 2, 1)          \
  F(Int16x8Sub, 2, 1)                  \
  F(Int16x8SubSaturate, 2, 1)          \
  F(Int16x8Mul, 2, 1)                  \
  F(Int16x8Min, 2, 1)                  \
  F(Int16x8Max, 2, 1)                  \
  F(Int16x8And, 2, 1)                  \
  F(Int16x8Or, 2, 1)                   \
  F(Int16x8Xor, 2, 1)                  \
  F(Int16x8Not, 1, 1)                  \
  F(Int16x8ShiftLeftByScalar, 2, 1)    \
  F(Int16x8ShiftRightByScalar, 2, 1)   \
  F(Int16x8Equal, 2, 1)                \
  F(Int16x8NotEqual, 2, 1)             \
  F(Int16x8LessThan, 2, 1)             \
  F(Int16x8LessThanOrEqual, 2, 1)      \
  F(Int16x8GreaterThan, 2, 1)          \
  F(Int16x8GreaterThanOrEqual, 2, 1)   \
  F(Int16x8Select, 3, 1)               \
  F(Int16x8Swizzle, 9, 1)              \
  F(Int16x8Shuffle, 10, 1)             \
  F(Int16x8FromUint16x8, 1, 1)         \
  F(Int16x8FromFloat32x4Bits, 1, 1)    \
  F(Int16x8FromInt32x4Bits, 1, 1)      \
  F(Int16x8FromUint32x4Bits, 1, 1)     \
  F(Int16x8FromUint16x8Bits, 1, 1)     \
  F(Int16x8FromInt8x16Bits, 1, 1)      \
  F(Int16x8FromUint8x16Bits, 1, 1)     \
  F(Int16x8Load, 2, 1)                 \
  F(Int16x8Store, 3, 1)                \
  F(Uint16x8Check, 1, 1)               \
  F(Uint16x8ExtractLane, 2, 1)         \
  F(Uint16x8ReplaceLane, 3, 1)         \
  F(Uint16x8Add, 2, 1)                 \
  F(Uint16x8AddSaturate, 2, 1)         \
  F(Uint16x8Sub, 2, 1)                 \
  F(Uint16x8SubSaturate, 2, 1)         \
  F(Uint16x8Mul, 2, 1)                 \
  F(Uint16x8Min, 2, 1)                 \
  F(Uint16x8Max, 2, 1)                 \
  F(Uint16x8And, 2, 1)                 \
  F(Uint16x8Or, 2, 1)                  \
  F(Uint16x8Xor, 2, 1)                 \
  F(Uint16x8Not, 1, 1)                 \
  F(Uint16x8ShiftLeftByScalar, 2, 1)   \
  F(Uint16x8ShiftRightByScalar, 2, 1)  \
  F(Uint16x8Equal, 2, 1)               \
  F(Uint16x8NotEqual, 2, 1)            \
  F(Uint16x8LessThan, 2, 1)            \
  F(Uint16x8LessThanOrEqual, 2, 1)     \
  F(Uint16x8GreaterThan, 2, 1)         \
  F(Uint16x8GreaterThanOrEqual, 2, 1)  \
  F(Uint16x8Select, 3, 1)              \
  F(Uint16x8Swizzle, 9, 1)             \
  F(Uint16x8Shuffle, 10, 1)            \
  F(Uint16x8FromInt16x8, 1, 1)         \
  F(Uint16x8FromFloat32x4Bits, 1, 1)   \
  F(Uint16x8FromInt32x4Bits, 1, 1)     \
  F(Uint16x8FromUint32x4Bits, 1, 1)    \
  F(Uint16x8FromInt16x8Bits, 1, 1)     \
  F(Uint16x8FromInt8x16Bits, 1, 1)     \
  F(Uint16x8FromUint8x16Bits, 1, 1)    \
  F(Uint16x8Load, 2, 1)                \
  F(Uint16x8Store, 3, 1)               \
  F(Bool16x8Check, 1, 1)               \
  F(Bool16x8ExtractLane, 2, 1)         \
  F(Bool16x8ReplaceLane, 3, 1)         \
  F(Bool16x8And, 2, 1)                 \
  F(Bool16x8Or, 2, 1)                  \
  F(Bool16x8Xor, 2, 1)                 \
  F(Bool16x8Not, 1, 1)                 \
  F(Bool16x8AnyTrue, 1, 1)             \
  F(Bool16x8AllTrue, 1, 1)             \
  F(Bool16x8Swizzle, 9, 1)             \
  F(Bool16x8Shuffle, 10, 1)            \
  F(Bool16x8Equal, 2, 1)               \
  F(Bool16x8NotEqual, 2, 1)            \
  F(Int8x16Check, 1, 1)                \
  F(Int8x16ExtractLane, 2, 1)          \
  F(Int8x16ReplaceLane, 3, 1)          \
  F(Int8x16Neg, 1, 1)                  \
  F(Int8x16Add, 2, 1)                  \
  F(Int8x16AddSaturate, 2, 1)          \
  F(Int8x16Sub, 2, 1)                  \
  F(Int8x16SubSaturate, 2, 1)          \
  F(Int8x16Mul, 2, 1)                  \
  F(Int8x16Min, 2, 1)                  \
  F(Int8x16Max, 2, 1)                  \
  F(Int8x16And, 2, 1)                  \
  F(Int8x16Or, 2, 1)                   \
  F(Int8x16Xor, 2, 1)                  \
  F(Int8x16Not, 1, 1)                  \
  F(Int8x16ShiftLeftByScalar, 2, 1)    \
  F(Int8x16ShiftRightByScalar, 2, 1)   \
  F(Int8x16Equal, 2, 1)                \
  F(Int8x16NotEqual, 2, 1)             \
  F(Int8x16LessThan, 2, 1)             \
  F(Int8x16LessThanOrEqual, 2, 1)      \
  F(Int8x16GreaterThan, 2, 1)          \
  F(Int8x16GreaterThanOrEqual, 2, 1)   \
  F(Int8x16Select, 3, 1)               \
  F(Int8x16Swizzle, 17, 1)             \
  F(Int8x16Shuffle, 18, 1)             \
  F(Int8x16FromUint8x16, 1, 1)         \
  F(Int8x16FromFloat32x4Bits, 1, 1)    \
  F(Int8x16FromInt32x4Bits, 1, 1)      \
  F(Int8x16FromUint32x4Bits, 1, 1)     \
  F(Int8x16FromInt16x8Bits, 1, 1)      \
  F(Int8x16FromUint16x8Bits, 1, 1)     \
  F(Int8x16FromUint8x16Bits, 1, 1)     \
  F(Int8x16Load, 2, 1)                 \
  F(Int8x16Store, 3, 1)                \
  F(Uint8x16Check, 1, 1)               \
  F(Uint8x16ExtractLane, 2, 1)         \
  F(Uint8x16ReplaceLane, 3, 1)         \
  F(Uint8x16Add, 2, 1)                 \
  F(Uint8x16AddSaturate, 2, 1)         \
  F(Uint8x16Sub, 2, 1)                 \
  F(Uint8x16SubSaturate, 2, 1)         \
  F(Uint8x16Mul, 2, 1)                 \
  F(Uint8x16Min, 2, 1)                 \
  F(Uint8x16Max, 2, 1)                 \
  F(Uint8x16And, 2, 1)                 \
  F(Uint8x16Or, 2, 1)                  \
  F(Uint8x16Xor, 2, 1)                 \
  F(Uint8x16Not, 1, 1)                 \
  F(Uint8x16ShiftLeftByScalar, 2, 1)   \
  F(Uint8x16ShiftRightByScalar, 2, 1)  \
  F(Uint8x16Equal, 2, 1)               \
  F(Uint8x16NotEqual, 2, 1)            \
  F(Uint8x16LessThan, 2, 1)            \
  F(Uint8x16LessThanOrEqual, 2, 1)     \
  F(Uint8x16GreaterThan, 2, 1)         \
  F(Uint8x16GreaterThanOrEqual, 2, 1)  \
  F(Uint8x16Select, 3, 1)              \
  F(Uint8x16Swizzle, 17, 1)            \
  F(Uint8x16Shuffle, 18, 1)            \
  F(Uint8x16FromInt8x16, 1, 1)         \
  F(Uint8x16FromFloat32x4Bits, 1, 1)   \
  F(Uint8x16FromInt32x4Bits, 1, 1)     \
  F(Uint8x16FromUint32x4Bits, 1, 1)    \
  F(Uint8x16FromInt16x8Bits, 1, 1)     \
  F(Uint8x16FromUint16x8Bits, 1, 1)    \
  F(Uint8x16FromInt8x16Bits, 1, 1)     \
  F(Uint8x16Load, 2, 1)                \
  F(Uint8x16Store, 3, 1)               \
  F(Bool8x16Check, 1, 1)               \
  F(Bool8x16ExtractLane, 2, 1)         \
  F(Bool8x16ReplaceLane, 3, 1)         \
  F(Bool8x16And, 2, 1)                 \
  F(Bool8x16Or, 2, 1)                  \
  F(Bool8x16Xor, 2, 1)                 \
  F(Bool8x16Not, 1, 1)                 \
  F(Bool8x16AnyTrue, 1, 1)             \
  F(Bool8x16AllTrue, 1, 1)             \
  F(Bool8x16Swizzle, 17, 1)            \
  F(Bool8x16Shuffle, 18, 1)            \
  F(Bool8x16Equal, 2, 1)               \
  F(Bool8x16NotEqual, 2, 1)

#define FOR_EACH_INTRINSIC_STRINGS(F)     \
  F(StringReplaceOneCharWithString, 3, 1) \
  F(StringIndexOf, 3, 1)                  \
  F(StringIndexOfUnchecked, 3, 1)         \
  F(StringLastIndexOf, 2, 1)              \
  F(SubString, 3, 1)                      \
  F(StringAdd, 2, 1)                      \
  F(InternalizeString, 1, 1)              \
  F(StringCharCodeAtRT, 2, 1)             \
  F(StringCompare, 2, 1)                  \
  F(StringBuilderConcat, 3, 1)            \
  F(StringBuilderJoin, 3, 1)              \
  F(SparseJoinWithSeparator, 3, 1)        \
  F(StringToArray, 2, 1)                  \
  F(StringToLowerCase, 1, 1)              \
  F(StringToUpperCase, 1, 1)              \
  F(StringLessThan, 2, 1)                 \
  F(StringLessThanOrEqual, 2, 1)          \
  F(StringGreaterThan, 2, 1)              \
  F(StringGreaterThanOrEqual, 2, 1)       \
  F(StringEqual, 2, 1)                    \
  F(StringNotEqual, 2, 1)                 \
  F(FlattenString, 1, 1)                  \
  F(StringCharFromCode, 1, 1)             \
  F(ExternalStringGetChar, 2, 1)          \
  F(StringCharCodeAt, 2, 1)

#define FOR_EACH_INTRINSIC_SYMBOL(F) \
  F(CreateSymbol, 1, 1)              \
  F(CreatePrivateSymbol, 1, 1)       \
  F(SymbolDescription, 1, 1)         \
  F(SymbolDescriptiveString, 1, 1)   \
  F(SymbolIsPrivate, 1, 1)

#define FOR_EACH_INTRINSIC_TEST(F)            \
  F(ConstructDouble, 2, 1)                    \
  F(DeoptimizeFunction, 1, 1)                 \
  F(DeoptimizeNow, 0, 1)                      \
  F(RunningInSimulator, 0, 1)                 \
  F(IsConcurrentRecompilationSupported, 0, 1) \
  F(OptimizeFunctionOnNextCall, -1, 1)        \
  F(InterpretFunctionOnNextCall, 1, 1)        \
  F(BaselineFunctionOnNextCall, 1, 1)         \
  F(OptimizeOsr, -1, 1)                       \
  F(NeverOptimizeFunction, 1, 1)              \
  F(GetOptimizationStatus, -1, 1)             \
  F(UnblockConcurrentRecompilation, 0, 1)     \
  F(GetOptimizationCount, 1, 1)               \
  F(GetUndetectable, 0, 1)                    \
  F(GetCallable, 0, 1)                        \
  F(ClearFunctionTypeFeedback, 1, 1)          \
  F(CheckWasmWrapperElision, 2, 1)            \
  F(NotifyContextDisposed, 0, 1)              \
  F(SetAllocationTimeout, -1 /* 2 || 3 */, 1) \
  F(DebugPrint, 1, 1)                         \
  F(DebugTrace, 0, 1)                         \
  F(GetExceptionDetails, 1, 1)                \
  F(GlobalPrint, 1, 1)                        \
  F(SystemBreak, 0, 1)                        \
  F(SetFlags, 1, 1)                           \
  F(Abort, 1, 1)                              \
  F(AbortJS, 1, 1)                            \
  F(NativeScriptsCount, 0, 1)                 \
  F(GetV8Version, 0, 1)                       \
  F(DisassembleFunction, 1, 1)                \
  F(TraceEnter, 0, 1)                         \
  F(TraceExit, 1, 1)                          \
  F(TraceTailCall, 0, 1)                      \
  F(HaveSameMap, 2, 1)                        \
  F(InNewSpace, 1, 1)                         \
  F(HasFastSmiElements, 1, 1)                 \
  F(HasFastObjectElements, 1, 1)              \
  F(HasFastSmiOrObjectElements, 1, 1)         \
  F(HasFastDoubleElements, 1, 1)              \
  F(HasFastHoleyElements, 1, 1)               \
  F(HasDictionaryElements, 1, 1)              \
  F(HasSloppyArgumentsElements, 1, 1)         \
  F(HasFixedTypedArrayElements, 1, 1)         \
  F(HasFastProperties, 1, 1)                  \
  F(HasFixedUint8Elements, 1, 1)              \
  F(HasFixedInt8Elements, 1, 1)               \
  F(HasFixedUint16Elements, 1, 1)             \
  F(HasFixedInt16Elements, 1, 1)              \
  F(HasFixedUint32Elements, 1, 1)             \
  F(HasFixedInt32Elements, 1, 1)              \
  F(HasFixedFloat32Elements, 1, 1)            \
  F(HasFixedFloat64Elements, 1, 1)            \
  F(HasFixedUint8ClampedElements, 1, 1)       \
  F(SpeciesProtector, 0, 1)                   \
  F(SerializeWasmModule, 1, 1)                \
  F(DeserializeWasmModule, 2, 1)              \
  F(IsAsmWasmCode, 1, 1)                      \
  F(IsWasmCode, 1, 1)                         \
  F(DisallowCodegenFromStrings, 0, 1)         \
  F(ValidateWasmInstancesChain, 2, 1)         \
  F(ValidateWasmModuleState, 1, 1)            \
  F(ValidateWasmOrphanedInstance, 1, 1)       \
  F(SetWasmCompileControls, 2, 1)             \
  F(SetWasmInstantiateControls, 0, 1)         \
  F(Verify, 1, 1)

#define FOR_EACH_INTRINSIC_TYPEDARRAY(F)     \
  F(ArrayBufferGetByteLength, 1, 1)          \
  F(ArrayBufferSliceImpl, 4, 1)              \
  F(ArrayBufferNeuter, 1, 1)                 \
  F(TypedArrayInitialize, 6, 1)              \
  F(TypedArrayInitializeFromArrayLike, 4, 1) \
  F(ArrayBufferViewGetByteLength, 1, 1)      \
  F(ArrayBufferViewGetByteOffset, 1, 1)      \
  F(TypedArrayGetLength, 1, 1)               \
  F(TypedArrayGetBuffer, 1, 1)               \
  F(TypedArraySetFastCases, 3, 1)            \
  F(TypedArrayMaxSizeInHeap, 0, 1)           \
  F(IsTypedArray, 1, 1)                      \
  F(IsSharedTypedArray, 1, 1)                \
  F(IsSharedIntegerTypedArray, 1, 1)         \
  F(IsSharedInteger32TypedArray, 1, 1)

#define FOR_EACH_INTRINSIC_WASM(F)           \
  F(WasmGrowMemory, 1, 1)                    \
  F(WasmMemorySize, 0, 1)                    \
  F(ThrowWasmError, 2, 1)                    \
  F(WasmThrowTypeError, 0, 1)                \
  F(WasmThrow, 2, 1)                         \
  F(WasmGetCaughtExceptionValue, 1, 1)       \
  F(ThrowWasmTrapUnreachable, 0, 1)          \
  F(ThrowWasmTrapMemOutOfBounds, 0, 1)       \
  F(ThrowWasmTrapDivByZero, 0, 1)            \
  F(ThrowWasmTrapDivUnrepresentable, 0, 1)   \
  F(ThrowWasmTrapRemByZero, 0, 1)            \
  F(ThrowWasmTrapFloatUnrepresentable, 0, 1) \
  F(ThrowWasmTrapFuncInvalid, 0, 1)          \
  F(ThrowWasmTrapFuncSigMismatch, 0, 1)      \
  F(WasmRunInterpreter, 3, 1)

#define FOR_EACH_INTRINSIC_RETURN_PAIR(F) \
  F(LoadLookupSlotForCall, 1, 2)

#define FOR_EACH_INTRINSIC_RETURN_TRIPLE(F) \
  F(ForInPrepare, 1, 3)

// Most intrinsics are implemented in the runtime/ directory, but ICs are
// implemented in ic.cc for now.
#define FOR_EACH_INTRINSIC_IC(F)             \
  F(BinaryOpIC_Miss, 2, 1)                   \
  F(BinaryOpIC_MissWithAllocationSite, 3, 1) \
  F(CallIC_Miss, 3, 1)                       \
  F(CompareIC_Miss, 3, 1)                    \
  F(ElementsTransitionAndStoreIC_Miss, 6, 1) \
  F(KeyedLoadIC_Miss, 4, 1)                  \
  F(KeyedLoadIC_MissFromStubFailure, 4, 1)   \
  F(KeyedStoreIC_Miss, 5, 1)                 \
  F(KeyedStoreIC_Slow, 5, 1)                 \
  F(LoadElementWithInterceptor, 2, 1)        \
  F(LoadGlobalIC_Miss, 3, 1)                 \
  F(LoadGlobalIC_Slow, 1, 1)                 \
  F(LoadIC_Miss, 4, 1)                       \
  F(LoadPropertyWithInterceptor, 3, 1)       \
  F(LoadPropertyWithInterceptorOnly, 3, 1)   \
  F(StoreCallbackProperty, 6, 1)             \
  F(StoreIC_Miss, 5, 1)                      \
  F(StorePropertyWithInterceptor, 3, 1)      \
  F(ToBooleanIC_Miss, 1, 1)                  \
  F(Unreachable, 0, 1)

#define FOR_EACH_INTRINSIC_RETURN_OBJECT(F) \
  FOR_EACH_INTRINSIC_IC(F)                  \
  FOR_EACH_INTRINSIC_ARRAY(F)               \
  FOR_EACH_INTRINSIC_ATOMICS(F)             \
  FOR_EACH_INTRINSIC_CLASSES(F)             \
  FOR_EACH_INTRINSIC_COLLECTIONS(F)         \
  FOR_EACH_INTRINSIC_COMPILER(F)            \
  FOR_EACH_INTRINSIC_DATE(F)                \
  FOR_EACH_INTRINSIC_DEBUG(F)               \
  FOR_EACH_INTRINSIC_ERROR(F)               \
  FOR_EACH_INTRINSIC_FORIN(F)               \
  FOR_EACH_INTRINSIC_INTERPRETER(F)         \
  FOR_EACH_INTRINSIC_FUNCTION(F)            \
  FOR_EACH_INTRINSIC_GENERATOR(F)           \
  FOR_EACH_INTRINSIC_I18N(F)                \
  FOR_EACH_INTRINSIC_INTERNAL(F)            \
  FOR_EACH_INTRINSIC_LITERALS(F)            \
  FOR_EACH_INTRINSIC_LIVEEDIT(F)            \
  FOR_EACH_INTRINSIC_MATHS(F)               \
  FOR_EACH_INTRINSIC_MODULE(F)              \
  FOR_EACH_INTRINSIC_NUMBERS(F)             \
  FOR_EACH_INTRINSIC_OBJECT(F)              \
  FOR_EACH_INTRINSIC_OPERATORS(F)           \
  FOR_EACH_INTRINSIC_PROXY(F)               \
  FOR_EACH_INTRINSIC_REGEXP(F)              \
  FOR_EACH_INTRINSIC_SCOPES(F)              \
  FOR_EACH_INTRINSIC_SIMD(F)                \
  FOR_EACH_INTRINSIC_STRINGS(F)             \
  FOR_EACH_INTRINSIC_SYMBOL(F)              \
  FOR_EACH_INTRINSIC_TEST(F)                \
  FOR_EACH_INTRINSIC_TYPEDARRAY(F)          \
  FOR_EACH_INTRINSIC_WASM(F)

// FOR_EACH_INTRINSIC defines the list of all intrinsics, coming in 2 flavors,
// either returning an object or a pair.
#define FOR_EACH_INTRINSIC(F)         \
  FOR_EACH_INTRINSIC_RETURN_TRIPLE(F) \
  FOR_EACH_INTRINSIC_RETURN_PAIR(F)   \
  FOR_EACH_INTRINSIC_RETURN_OBJECT(F)


#define F(name, nargs, ressize)                                 \
  Object* Runtime_##name(int args_length, Object** args_object, \
                         Isolate* isolate);
FOR_EACH_INTRINSIC_RETURN_OBJECT(F)
#undef F

//---------------------------------------------------------------------------
// Runtime provides access to all C++ runtime functions.

class Runtime : public AllStatic {
 public:
  enum FunctionId : int32_t {
#define F(name, nargs, ressize) k##name,
#define I(name, nargs, ressize) kInline##name,
    FOR_EACH_INTRINSIC(F) FOR_EACH_INTRINSIC(I)
#undef I
#undef F
        kNumFunctions,
  };

  enum IntrinsicType { RUNTIME, INLINE };

  // Intrinsic function descriptor.
  struct Function {
    FunctionId function_id;
    IntrinsicType intrinsic_type;
    // The JS name of the function.
    const char* name;

    // For RUNTIME functions, this is the C++ entry point.
    // For INLINE functions this is the C++ entry point of the fall back.
    Address entry;

    // The number of arguments expected. nargs is -1 if the function takes
    // a variable number of arguments.
    int8_t nargs;
    // Size of result.  Most functions return a single pointer, size 1.
    int8_t result_size;
  };

  static const int kNotFound = -1;

  // Get the intrinsic function with the given name.
  static const Function* FunctionForName(const unsigned char* name, int length);

  // Get the intrinsic function with the given FunctionId.
  V8_EXPORT_PRIVATE static const Function* FunctionForId(FunctionId id);

  // Get the intrinsic function with the given function entry address.
  static const Function* FunctionForEntry(Address ref);

  // Get the runtime intrinsic function table.
  static const Function* RuntimeFunctionTable(Isolate* isolate);

  MUST_USE_RESULT static Maybe<bool> DeleteObjectProperty(
      Isolate* isolate, Handle<JSReceiver> receiver, Handle<Object> key,
      LanguageMode language_mode);

  MUST_USE_RESULT static MaybeHandle<Object> SetObjectProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key,
      Handle<Object> value, LanguageMode language_mode);

  MUST_USE_RESULT static MaybeHandle<Object> GetObjectProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key,
      bool* is_found_out = nullptr);

  enum TypedArrayId {
    // arrayIds below should be synchronized with typedarray.js natives.
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
                                   ElementsKind* fixed_elements_kind,
                                   size_t* element_size);

  static MaybeHandle<JSArray> GetInternalProperties(Isolate* isolate,
                                                    Handle<Object>);
};


class RuntimeState {
 public:
  unibrow::Mapping<unibrow::ToUppercase, 128>* to_upper_mapping() {
    return &to_upper_mapping_;
  }
  unibrow::Mapping<unibrow::ToLowercase, 128>* to_lower_mapping() {
    return &to_lower_mapping_;
  }

  Runtime::Function* redirected_intrinsic_functions() {
    return redirected_intrinsic_functions_.get();
  }

  void set_redirected_intrinsic_functions(
      Runtime::Function* redirected_intrinsic_functions) {
    redirected_intrinsic_functions_.reset(redirected_intrinsic_functions);
  }

 private:
  RuntimeState() {}
  unibrow::Mapping<unibrow::ToUppercase, 128> to_upper_mapping_;
  unibrow::Mapping<unibrow::ToLowercase, 128> to_lower_mapping_;

  std::unique_ptr<Runtime::Function[]> redirected_intrinsic_functions_;

  friend class Isolate;
  friend class Runtime;

  DISALLOW_COPY_AND_ASSIGN(RuntimeState);
};

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream&, Runtime::FunctionId);

//---------------------------------------------------------------------------
// Constants used by interface to runtime functions.

class AllocateDoubleAlignFlag : public BitField<bool, 0, 1> {};
class AllocateTargetSpace : public BitField<AllocationSpace, 1, 3> {};

class DeclareGlobalsEvalFlag : public BitField<bool, 0, 1> {};
class DeclareGlobalsNativeFlag : public BitField<bool, 1, 1> {};
STATIC_ASSERT(LANGUAGE_END == 2);
class DeclareGlobalsLanguageMode : public BitField<LanguageMode, 2, 1> {};

}  // namespace internal
}  // namespace v8

#endif  // V8_RUNTIME_RUNTIME_H_
