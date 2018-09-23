// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RUNTIME_RUNTIME_H_
#define V8_RUNTIME_RUNTIME_H_

#include <memory>

#include "src/allocation.h"
#include "src/base/platform/time.h"
#include "src/elements-kind.h"
#include "src/globals.h"
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

#define FOR_EACH_INTRINSIC_ARRAY(F) \
  F(ArrayIncludes_Slow, 3, 1)       \
  F(ArrayIndexOf, 3, 1)             \
  F(ArrayIsArray, 1, 1)             \
  F(ArraySpeciesConstructor, 1, 1)  \
  F(EstimateNumberOfElements, 1, 1) \
  F(GetArrayKeys, 2, 1)             \
  F(GrowArrayElements, 2, 1)        \
  F(HasComplexElements, 1, 1)       \
  F(IsArray, 1, 1)                  \
  F(MoveArrayContents, 2, 1)        \
  F(NewArray, -1 /* >= 3 */, 1)     \
  F(NormalizeElements, 1, 1)        \
  F(PrepareElementsForSort, 2, 1)   \
  F(TransitionElementsKind, 2, 1)   \
  F(TrySliceSimpleNonFastElements, 3, 1)

#define FOR_EACH_INTRINSIC_ATOMICS(F)  \
  F(AtomicsAdd, 3, 1)                  \
  F(AtomicsAnd, 3, 1)                  \
  F(AtomicsCompareExchange, 4, 1)      \
  F(AtomicsExchange, 3, 1)             \
  F(AtomicsNumWaitersForTesting, 2, 1) \
  F(AtomicsOr, 3, 1)                   \
  F(AtomicsSub, 3, 1)                  \
  F(AtomicsXor, 3, 1)                  \
  F(SetAllowAtomicsWait, 1, 1)

#define FOR_EACH_INTRINSIC_BIGINT(F) \
  F(BigIntBinaryOp, 3, 1)            \
  F(BigIntCompareToBigInt, 3, 1)     \
  F(BigIntCompareToNumber, 3, 1)     \
  F(BigIntCompareToString, 3, 1)     \
  F(BigIntEqualToBigInt, 2, 1)       \
  F(BigIntEqualToNumber, 2, 1)       \
  F(BigIntEqualToString, 2, 1)       \
  F(BigIntToBoolean, 1, 1)           \
  F(BigIntToNumber, 1, 1)            \
  F(BigIntUnaryOp, 2, 1)             \
  F(ToBigInt, 1, 1)

#define FOR_EACH_INTRINSIC_CLASSES(F)       \
  F(DefineClass, -1 /* >= 3 */, 1)          \
  F(HomeObjectSymbol, 0, 1)                 \
  F(LoadFromSuper, 3, 1)                    \
  F(LoadKeyedFromSuper, 3, 1)               \
  F(StoreKeyedToSuper_Sloppy, 4, 1)         \
  F(StoreKeyedToSuper_Strict, 4, 1)         \
  F(StoreToSuper_Sloppy, 4, 1)              \
  F(StoreToSuper_Strict, 4, 1)              \
  F(ThrowConstructorNonCallableError, 1, 1) \
  F(ThrowNotSuperConstructor, 2, 1)         \
  F(ThrowStaticPrototypeError, 0, 1)        \
  F(ThrowSuperAlreadyCalledError, 0, 1)     \
  F(ThrowSuperNotCalled, 0, 1)              \
  F(ThrowUnsupportedSuperError, 0, 1)

#define FOR_EACH_INTRINSIC_COLLECTIONS(F) \
  F(GetWeakMapEntries, 2, 1)              \
  F(GetWeakSetValues, 2, 1)               \
  F(MapGrow, 1, 1)                        \
  F(MapIteratorClone, 1, 1)               \
  F(MapShrink, 1, 1)                      \
  F(SetGrow, 1, 1)                        \
  F(SetIteratorClone, 1, 1)               \
  F(SetShrink, 1, 1)                      \
  F(TheHole, 0, 1)                        \
  F(WeakCollectionDelete, 3, 1)           \
  F(WeakCollectionSet, 4, 1)

#define FOR_EACH_INTRINSIC_COMPILER(F)    \
  F(CompileForOnStackReplacement, 1, 1)   \
  F(CompileLazy, 1, 1)                    \
  F(CompileOptimized_Concurrent, 1, 1)    \
  F(CompileOptimized_NotConcurrent, 1, 1) \
  F(EvictOptimizedCodeSlot, 1, 1)         \
  F(FunctionFirstExecution, 1, 1)         \
  F(InstantiateAsmJs, 4, 1)               \
  F(NotifyDeoptimized, 0, 1)              \
  F(ResolvePossiblyDirectEval, 6, 1)

#define FOR_EACH_INTRINSIC_DATE(F) \
  F(DateCurrentTime, 0, 1)         \
  F(IsDate, 1, 1)

#define FOR_EACH_INTRINSIC_DEBUG(F)             \
  F(ClearStepping, 0, 1)                        \
  F(CollectGarbage, 1, 1)                       \
  F(DebugBreakAtEntry, 1, 1)                    \
  F(DebugCollectCoverage, 0, 1)                 \
  F(DebugGetLoadedScriptIds, 0, 1)              \
  F(DebugIsActive, 0, 1)                        \
  F(DebugOnFunctionCall, 2, 1)                  \
  F(DebugPopPromise, 0, 1)                      \
  F(DebugPrepareStepInSuspendedGenerator, 0, 1) \
  F(DebugPushPromise, 1, 1)                     \
  F(DebugAsyncFunctionSuspended, 1, 1)          \
  F(DebugAsyncFunctionFinished, 2, 1)           \
  F(DebugToggleBlockCoverage, 1, 1)             \
  F(DebugTogglePreciseCoverage, 1, 1)           \
  F(FunctionGetInferredName, 1, 1)              \
  F(GetBreakLocations, 1, 1)                    \
  F(GetGeneratorScopeCount, 1, 1)               \
  F(GetGeneratorScopeDetails, 2, 1)             \
  F(GetHeapUsage, 0, 1)                         \
  F(HandleDebuggerStatement, 0, 1)              \
  F(IncBlockCounter, 2, 1)                      \
  F(IsBreakOnException, 1, 1)                   \
  F(ScheduleBreak, 0, 1)                        \
  F(ScriptLocationFromLine2, 4, 1)              \
  F(SetGeneratorScopeVariableValue, 4, 1)       \
  F(LiveEditPatchScript, 2, 1)

#define FOR_EACH_INTRINSIC_FORIN(F) \
  F(ForInEnumerate, 1, 1)           \
  F(ForInHasProperty, 2, 1)

#ifdef V8_TRACE_IGNITION
#define FOR_EACH_INTRINSIC_INTERPRETER_TRACE(F) \
  F(InterpreterTraceBytecodeEntry, 3, 1)        \
  F(InterpreterTraceBytecodeExit, 3, 1)
#else
#define FOR_EACH_INTRINSIC_INTERPRETER_TRACE(F)
#endif

#ifdef V8_TRACE_FEEDBACK_UPDATES
#define FOR_EACH_INTRINSIC_INTERPRETER_TRACE_FEEDBACK(F) \
  F(InterpreterTraceUpdateFeedback, 3, 1)
#else
#define FOR_EACH_INTRINSIC_INTERPRETER_TRACE_FEEDBACK(F)
#endif

#define FOR_EACH_INTRINSIC_INTERPRETER(F)          \
  FOR_EACH_INTRINSIC_INTERPRETER_TRACE(F)          \
  FOR_EACH_INTRINSIC_INTERPRETER_TRACE_FEEDBACK(F) \
  F(InterpreterDeserializeLazy, 2, 1)

#define FOR_EACH_INTRINSIC_FUNCTION(F)     \
  F(Call, -1 /* >= 2 */, 1)                \
  F(FunctionGetName, 1, 1)                 \
  F(FunctionGetScriptSource, 1, 1)         \
  F(FunctionGetScriptId, 1, 1)             \
  F(FunctionGetScriptSourcePosition, 1, 1) \
  F(FunctionGetSourceCode, 1, 1)           \
  F(FunctionIsAPIFunction, 1, 1)           \
  F(IsConstructor, 1, 1)                   \
  F(IsFunction, 1, 1)                      \
  F(SetCode, 2, 1)                         \
  F(SetNativeFlag, 1, 1)

#define FOR_EACH_INTRINSIC_GENERATOR(F)       \
  F(AsyncGeneratorHasCatchHandlerForPC, 1, 1) \
  F(AsyncGeneratorReject, 2, 1)               \
  F(AsyncGeneratorResolve, 3, 1)              \
  F(AsyncGeneratorYield, 3, 1)                \
  F(CreateJSGeneratorObject, 2, 1)            \
  F(GeneratorClose, 1, 1)                     \
  F(GeneratorGetFunction, 1, 1)               \
  F(GeneratorGetInputOrDebugPos, 1, 1)        \
  F(GeneratorGetResumeMode, 1, 1)

#ifdef V8_INTL_SUPPORT
#define FOR_EACH_INTRINSIC_INTL(F)           \
  F(AvailableLocalesOf, 1, 1)                \
  F(BreakIteratorBreakType, 1, 1)            \
  F(BreakIteratorCurrent, 1, 1)              \
  F(BreakIteratorFirst, 1, 1)                \
  F(BreakIteratorNext, 1, 1)                 \
  F(CanonicalizeLanguageTag, 1, 1)           \
  F(CollatorResolvedOptions, 1, 1)           \
  F(CreateBreakIterator, 3, 1)               \
  F(CreateDateTimeFormat, 3, 1)              \
  F(CreateNumberFormat, 3, 1)                \
  F(CurrencyDigits, 1, 1)                    \
  F(DateCacheVersion, 0, 1)                  \
  F(DefaultNumberOption, 5, 1)               \
  F(DefineWEProperty, 3, 1)                  \
  F(FormatList, 2, 1)                        \
  F(FormatListToParts, 2, 1)                 \
  F(GetDefaultICULocale, 0, 1)               \
  F(GetNumberOption, 5, 1)                   \
  F(IntlUnwrapReceiver, 5, 1)                \
  F(IsInitializedIntlObjectOfType, 2, 1)     \
  F(IsWellFormedCurrencyCode, 1, 1)          \
  F(MarkAsInitializedIntlObjectOfType, 2, 1) \
  F(ParseExtension, 1, 1)                    \
  F(PluralRulesResolvedOptions, 1, 1)        \
  F(PluralRulesSelect, 2, 1)                 \
  F(ToDateTimeOptions, 3, 1)                 \
  F(ToLocaleDateTime, 6, 1)                  \
  F(StringToLowerCaseIntl, 1, 1)             \
  F(StringToUpperCaseIntl, 1, 1)             \
  F(SupportedLocalesOf, 3, 1)                \
// End of macro.
#else
#define FOR_EACH_INTRINSIC_INTL(F)
#endif  // V8_INTL_SUPPORT

#define FOR_EACH_INTRINSIC_INTERNAL(F)                               \
  F(AllocateInNewSpace, 1, 1)                                        \
  F(AllocateInTargetSpace, 2, 1)                                     \
  F(AllocateSeqOneByteString, 1, 1)                                  \
  F(AllocateSeqTwoByteString, 1, 1)                                  \
  F(AllowDynamicFunction, 1, 1)                                      \
  F(CheckIsBootstrapping, 0, 1)                                      \
  F(CreateAsyncFromSyncIterator, 1, 1)                               \
  F(CreateListFromArrayLike, 1, 1)                                   \
  F(CreateTemplateObject, 1, 1)                                      \
  F(DeserializeLazy, 1, 1)                                           \
  F(ExportFromRuntime, 1, 1)                                         \
  F(GetAndResetRuntimeCallStats, -1 /* <= 2 */, 1)                   \
  F(IncrementUseCounter, 1, 1)                                       \
  F(InstallToContext, 1, 1)                                          \
  F(Interrupt, 0, 1)                                                 \
  F(IS_VAR, 1, 1)                                                    \
  F(NewReferenceError, 2, 1)                                         \
  F(NewSyntaxError, 2, 1)                                            \
  F(NewTypeError, 2, 1)                                              \
  F(OrdinaryHasInstance, 2, 1)                                       \
  F(PromoteScheduledException, 0, 1)                                 \
  F(ReportMessage, 1, 1)                                             \
  F(ReThrow, 1, 1)                                                   \
  F(RunMicrotaskCallback, 2, 1)                                      \
  F(RunMicrotasks, 0, 1)                                             \
  F(StackGuard, 0, 1)                                                \
  F(Throw, 1, 1)                                                     \
  F(ThrowApplyNonFunction, 1, 1)                                     \
  F(ThrowCalledNonCallable, 1, 1)                                    \
  F(ThrowConstructedNonConstructable, 1, 1)                          \
  F(ThrowConstructorReturnedNonObject, 0, 1)                         \
  F(ThrowInvalidStringLength, 0, 1)                                  \
  F(ThrowInvalidTypedArrayAlignment, 2, 1)                           \
  F(ThrowIteratorResultNotAnObject, 1, 1)                            \
  F(ThrowNotConstructor, 1, 1)                                       \
  F(ThrowRangeError, -1 /* >= 1 */, 1)                               \
  F(ThrowReferenceError, 1, 1)                                       \
  F(ThrowStackOverflow, 0, 1)                                        \
  F(ThrowSymbolAsyncIteratorInvalid, 0, 1)                           \
  F(ThrowSymbolIteratorInvalid, 0, 1)                                \
  F(ThrowThrowMethodMissing, 0, 1)                                   \
  F(ThrowTypeError, -1 /* >= 1 */, 1)                                \
  F(Typeof, 1, 1)                                                    \
  F(UnwindAndFindExceptionHandler, 0, 1)

#define FOR_EACH_INTRINSIC_LITERALS(F)              \
  F(CreateArrayLiteral, 4, 1)                       \
  F(CreateArrayLiteralWithoutAllocationSite, 2, 1)  \
  F(CreateObjectLiteral, 4, 1)                      \
  F(CreateObjectLiteralWithoutAllocationSite, 2, 1) \
  F(CreateRegExpLiteral, 4, 1)

#define FOR_EACH_INTRINSIC_MATHS(F) F(GenerateRandomNumbers, 0, 1)

#define FOR_EACH_INTRINSIC_MODULE(F) \
  F(DynamicImportCall, 2, 1)         \
  F(GetImportMetaObject, 0, 1)       \
  F(GetModuleNamespace, 1, 1)

#define FOR_EACH_INTRINSIC_NUMBERS(F) \
  F(GetHoleNaNLower, 0, 1)            \
  F(GetHoleNaNUpper, 0, 1)            \
  F(IsSmi, 1, 1)                      \
  F(IsValidSmi, 1, 1)                 \
  F(MaxSmi, 0, 1)                     \
  F(NumberToString, 1, 1)             \
  F(SmiLexicographicCompare, 2, 1)    \
  F(StringParseFloat, 1, 1)           \
  F(StringParseInt, 2, 1)             \
  F(StringToNumber, 1, 1)

#define FOR_EACH_INTRINSIC_OBJECT(F)                            \
  F(AddDictionaryProperty, 3, 1)                                \
  F(AddElement, 3, 1)                                           \
  F(AddNamedProperty, 4, 1)                                     \
  F(AddPrivateField, 3, 1)                                      \
  F(AllocateHeapNumber, 0, 1)                                   \
  F(ClassOf, 1, 1)                                              \
  F(CollectTypeProfile, 3, 1)                                   \
  F(CompleteInobjectSlackTrackingForMap, 1, 1)                  \
  F(CopyDataProperties, 2, 1)                                   \
  F(CopyDataPropertiesWithExcludedProperties, -1 /* >= 1 */, 1) \
  F(CreateDataProperty, 3, 1)                                   \
  F(CreateIterResultObject, 2, 1)                               \
  F(DefineAccessorPropertyUnchecked, 5, 1)                      \
  F(DefineDataPropertyInLiteral, 6, 1)                          \
  F(DefineGetterPropertyUnchecked, 4, 1)                        \
  F(DefineMethodsInternal, 3, 1)                                \
  F(DefineSetterPropertyUnchecked, 4, 1)                        \
  F(DeleteProperty, 3, 1)                                       \
  F(GetFunctionName, 1, 1)                                      \
  F(GetOwnPropertyDescriptor, 2, 1)                             \
  F(GetOwnPropertyKeys, 2, 1)                                   \
  F(GetProperty, 2, 1)                                          \
  F(GetPrototype, 1, 1)                                         \
  F(HasFastPackedElements, 1, 1)                                \
  F(HasInPrototypeChain, 2, 1)                                  \
  F(HasProperty, 2, 1)                                          \
  F(InternalSetPrototype, 2, 1)                                 \
  F(IsJSReceiver, 1, 1)                                         \
  F(KeyedGetProperty, 2, 1)                                     \
  F(NewObject, 2, 1)                                            \
  F(ObjectCreate, 2, 1)                                         \
  F(ObjectEntries, 1, 1)                                        \
  F(ObjectEntriesSkipFastPath, 1, 1)                            \
  F(ObjectHasOwnProperty, 2, 1)                                 \
  F(ObjectKeys, 1, 1)                                           \
  F(ObjectGetOwnPropertyNames, 1, 1)                            \
  F(ObjectGetOwnPropertyNamesTryFast, 1, 1)                     \
  F(ObjectValues, 1, 1)                                         \
  F(ObjectValuesSkipFastPath, 1, 1)                             \
  F(OptimizeObjectForAddingMultipleProperties, 2, 1)            \
  F(SameValue, 2, 1)                                            \
  F(SameValueZero, 2, 1)                                        \
  F(SetDataProperties, 2, 1)                                    \
  F(SetProperty, 4, 1)                                          \
  F(ShrinkPropertyDictionary, 1, 1)                             \
  F(ToFastProperties, 1, 1)                                     \
  F(ToInteger, 1, 1)                                            \
  F(ToLength, 1, 1)                                             \
  F(ToName, 1, 1)                                               \
  F(ToNumber, 1, 1)                                             \
  F(ToNumeric, 1, 1)                                            \
  F(ToObject, 1, 1)                                             \
  F(ToPrimitive, 1, 1)                                          \
  F(ToPrimitive_Number, 1, 1)                                   \
  F(ToString, 1, 1)                                             \
  F(TryMigrateInstance, 1, 1)                                   \
  F(ValueOf, 1, 1)

#define FOR_EACH_INTRINSIC_OPERATORS(F) \
  F(Add, 2, 1)                          \
  F(Equal, 2, 1)                        \
  F(GreaterThan, 2, 1)                  \
  F(GreaterThanOrEqual, 2, 1)           \
  F(LessThan, 2, 1)                     \
  F(LessThanOrEqual, 2, 1)              \
  F(NotEqual, 2, 1)                     \
  F(StrictEqual, 2, 1)                  \
  F(StrictNotEqual, 2, 1)

#define FOR_EACH_INTRINSIC_PROMISE(F)  \
  F(EnqueueMicrotask, 1, 1)            \
  F(PromiseHookAfter, 1, 1)            \
  F(PromiseHookBefore, 1, 1)           \
  F(PromiseHookInit, 2, 1)             \
  F(AwaitPromisesInit, 3, 1)           \
  F(PromiseMarkAsHandled, 1, 1)        \
  F(PromiseRejectEventFromStack, 2, 1) \
  F(PromiseResult, 1, 1)               \
  F(PromiseRevokeReject, 1, 1)         \
  F(PromiseStatus, 1, 1)               \
  F(RejectPromise, 3, 1)               \
  F(ResolvePromise, 2, 1)              \
  F(PromiseRejectAfterResolved, 2, 1)  \
  F(PromiseResolveAfterResolved, 2, 1)

#define FOR_EACH_INTRINSIC_PROXY(F)   \
  F(CheckProxyGetSetTrapResult, 2, 1) \
  F(CheckProxyHasTrap, 2, 1)          \
  F(GetPropertyWithReceiver, 3, 1)    \
  F(IsJSProxy, 1, 1)                  \
  F(JSProxyGetHandler, 1, 1)          \
  F(JSProxyGetTarget, 1, 1)           \
  F(SetPropertyWithReceiver, 5, 1)

#define FOR_EACH_INTRINSIC_REGEXP(F)                \
  F(IsRegExp, 1, 1)                                 \
  F(RegExpExec, 4, 1)                               \
  F(RegExpExecMultiple, 4, 1)                       \
  F(RegExpInitializeAndCompile, 3, 1)               \
  F(RegExpInternalReplace, 3, 1)                    \
  F(RegExpReplace, 3, 1)                            \
  F(RegExpSplit, 3, 1)                              \
  F(StringReplaceNonGlobalRegExpWithFunction, 3, 1) \
  F(StringSplit, 3, 1)

#define FOR_EACH_INTRINSIC_SCOPES(F)      \
  F(DeclareEvalFunction, 2, 1)            \
  F(DeclareEvalVar, 1, 1)                 \
  F(DeclareGlobals, 3, 1)                 \
  F(DeleteLookupSlot, 1, 1)               \
  F(LoadLookupSlot, 1, 1)                 \
  F(LoadLookupSlotInsideTypeof, 1, 1)     \
  F(NewArgumentsElements, 3, 1)           \
                                          \
  F(NewClosure, 2, 1)                     \
  F(NewClosure_Tenured, 2, 1)             \
  F(NewFunctionContext, 1, 1)             \
  F(NewRestParameter, 1, 1)               \
  F(NewScriptContext, 1, 1)               \
  F(NewSloppyArguments, 3, 1)             \
  F(NewSloppyArguments_Generic, 1, 1)     \
  F(NewStrictArguments, 1, 1)             \
  F(PushBlockContext, 1, 1)               \
  F(PushCatchContext, 2, 1)               \
  F(PushModuleContext, 2, 1)              \
  F(PushWithContext, 2, 1)                \
  F(StoreLookupSlot_Sloppy, 2, 1)         \
  F(StoreLookupSlot_SloppyHoisting, 2, 1) \
  F(StoreLookupSlot_Strict, 2, 1)         \
  F(ThrowConstAssignError, 0, 1)

#define FOR_EACH_INTRINSIC_STRINGS(F)     \
  F(FlattenString, 1, 1)                  \
  F(GetSubstitution, 5, 1)                \
  F(InternalizeString, 1, 1)              \
  F(SparseJoinWithSeparator, 3, 1)        \
  F(StringAdd, 2, 1)                      \
  F(StringBuilderConcat, 3, 1)            \
  F(StringBuilderJoin, 3, 1)              \
  F(StringCharCodeAt, 2, 1)               \
  F(StringCharFromCode, 1, 1)             \
  F(StringEqual, 2, 1)                    \
  F(StringGreaterThan, 2, 1)              \
  F(StringGreaterThanOrEqual, 2, 1)       \
  F(StringIncludes, 3, 1)                 \
  F(StringIndexOf, 3, 1)                  \
  F(StringIndexOfUnchecked, 3, 1)         \
  F(StringLastIndexOf, 2, 1)              \
  F(StringLessThan, 2, 1)                 \
  F(StringLessThanOrEqual, 2, 1)          \
  F(StringMaxLength, 0, 1)                \
  F(StringNotEqual, 2, 1)                 \
  F(StringReplaceOneCharWithString, 3, 1) \
  F(StringSubstring, 3, 1)                \
  F(StringToArray, 2, 1)                  \
  F(StringTrim, 2, 1)

#define FOR_EACH_INTRINSIC_SYMBOL(F)       \
  F(CreatePrivateFieldSymbol, 0, 1)        \
  F(CreatePrivateSymbol, -1 /* <= 1 */, 1) \
  F(SymbolDescriptiveString, 1, 1)         \
  F(SymbolIsPrivate, 1, 1)

#define FOR_EACH_INTRINSIC_TEST(F)            \
  F(Abort, 1, 1)                              \
  F(AbortJS, 1, 1)                            \
  F(ClearFunctionFeedback, 1, 1)              \
  F(CompleteInobjectSlackTracking, 1, 1)      \
  F(ConstructConsString, 2, 1)                \
  F(ConstructSlicedString, 2, 1)              \
  F(ConstructDouble, 2, 1)                    \
  F(DebugPrint, 1, 1)                         \
  F(DebugTrace, 0, 1)                         \
  F(DebugTrackRetainingPath, -1, 1)           \
  F(DeoptimizeFunction, 1, 1)                 \
  F(DeoptimizeNow, 0, 1)                      \
  F(DeserializeWasmModule, 2, 1)              \
  F(DisallowCodegenFromStrings, 1, 1)         \
  F(DisallowWasmCodegen, 1, 1)                \
  F(DisassembleFunction, 1, 1)                \
  F(FreezeWasmLazyCompilation, 1, 1)          \
  F(GetCallable, 0, 1)                        \
  F(GetDeoptCount, 1, 1)                      \
  F(GetOptimizationStatus, -1, 1)             \
  F(GetUndetectable, 0, 1)                    \
  F(GetWasmRecoveredTrapCount, 0, 1)          \
  F(GlobalPrint, 1, 1)                        \
  F(HasDictionaryElements, 1, 1)              \
  F(HasDoubleElements, 1, 1)                  \
  F(HasFastElements, 1, 1)                    \
  F(HasFastProperties, 1, 1)                  \
  F(HasFixedBigInt64Elements, 1, 1)           \
  F(HasFixedBigUint64Elements, 1, 1)          \
  F(HasFixedFloat32Elements, 1, 1)            \
  F(HasFixedFloat64Elements, 1, 1)            \
  F(HasFixedInt16Elements, 1, 1)              \
  F(HasFixedInt32Elements, 1, 1)              \
  F(HasFixedInt8Elements, 1, 1)               \
  F(HasFixedUint16Elements, 1, 1)             \
  F(HasFixedUint32Elements, 1, 1)             \
  F(HasFixedUint8ClampedElements, 1, 1)       \
  F(HasFixedUint8Elements, 1, 1)              \
  F(HasHoleyElements, 1, 1)                   \
  F(HasObjectElements, 1, 1)                  \
  F(HasSloppyArgumentsElements, 1, 1)         \
  F(HasSmiElements, 1, 1)                     \
  F(HasSmiOrObjectElements, 1, 1)             \
  F(HaveSameMap, 2, 1)                        \
  F(HeapObjectVerify, 1, 1)                   \
  F(InNewSpace, 1, 1)                         \
  F(IsAsmWasmCode, 1, 1)                      \
  F(IsConcurrentRecompilationSupported, 0, 1) \
  F(WasmTierUpFunction, 2, 1)                 \
  F(IsLiftoffFunction, 1, 1)                  \
  F(IsWasmCode, 1, 1)                         \
  F(IsWasmTrapHandlerEnabled, 0, 1)           \
  F(NeverOptimizeFunction, 1, 1)              \
  F(NotifyContextDisposed, 0, 1)              \
  F(OptimizeFunctionOnNextCall, -1, 1)        \
  F(OptimizeOsr, -1, 1)                       \
  F(PrintWithNameForAssert, 2, 1)             \
  F(RedirectToWasmInterpreter, 2, 1)          \
  F(RunningInSimulator, 0, 1)                 \
  F(SerializeWasmModule, 1, 1)                \
  F(SetAllocationTimeout, -1 /* 2 || 3 */, 1) \
  F(SetForceSlowPath, 1, 1)                   \
  F(SetWasmCompileControls, 2, 1)             \
  F(SetWasmInstantiateControls, 0, 1)         \
  F(ArraySpeciesProtector, 0, 1)              \
  F(TypedArraySpeciesProtector, 0, 1)         \
  F(PromiseSpeciesProtector, 0, 1)            \
  F(SystemBreak, 0, 1)                        \
  F(TraceEnter, 0, 1)                         \
  F(TraceExit, 1, 1)                          \
  F(UnblockConcurrentRecompilation, 0, 1)     \
  F(WasmGetNumberOfInstances, 1, 1)           \
  F(WasmNumInterpretedCalls, 1, 1)            \
  F(WasmTraceMemory, 1, 1)                    \
  F(WasmMemoryHasFullGuardRegion, 1, 1)       \
  F(SetWasmThreadsEnabled, 1, 1)

#define FOR_EACH_INTRINSIC_TYPEDARRAY(F) \
  F(ArrayBufferNeuter, 1, 1)             \
  F(ArrayBufferViewWasNeutered, 1, 1)    \
  F(IsTypedArray, 1, 1)                  \
  F(TypedArrayCopyElements, 3, 1)        \
  F(TypedArrayGetBuffer, 1, 1)           \
  F(TypedArrayGetLength, 1, 1)           \
  F(TypedArraySet, 2, 1)                 \
  F(TypedArraySortFast, 1, 1)

#define FOR_EACH_INTRINSIC_WASM(F)   \
  F(ThrowWasmError, 1, 1)            \
  F(ThrowWasmStackOverflow, 0, 1)    \
  F(WasmExceptionGetElement, 1, 1)   \
  F(WasmExceptionSetElement, 2, 1)   \
  F(WasmGetExceptionRuntimeId, 0, 1) \
  F(WasmGrowMemory, 2, 1)            \
  F(WasmRunInterpreter, 2, 1)        \
  F(WasmStackGuard, 0, 1)            \
  F(WasmThrow, 0, 1)                 \
  F(WasmThrowCreate, 2, 1)           \
  F(WasmThrowTypeError, 0, 1)        \
  F(WasmCompileLazy, 2, 1)

#define FOR_EACH_INTRINSIC_RETURN_PAIR(F) \
  F(DebugBreakOnBytecode, 1, 2)           \
  F(LoadLookupSlotForCall, 1, 2)

// Most intrinsics are implemented in the runtime/ directory, but ICs are
// implemented in ic.cc for now.
#define FOR_EACH_INTRINSIC_IC(F)             \
  F(ElementsTransitionAndStoreIC_Miss, 6, 1) \
  F(KeyedLoadIC_Miss, 4, 1)                  \
  F(KeyedStoreIC_Miss, 5, 1)                 \
  F(KeyedStoreIC_Slow, 5, 1)                 \
  F(LoadAccessorProperty, 4, 1)              \
  F(LoadCallbackProperty, 4, 1)              \
  F(LoadElementWithInterceptor, 2, 1)        \
  F(LoadGlobalIC_Miss, 3, 1)                 \
  F(LoadGlobalIC_Slow, 3, 1)                 \
  F(LoadIC_Miss, 4, 1)                       \
  F(LoadPropertyWithInterceptor, 5, 1)       \
  F(StoreCallbackProperty, 6, 1)             \
  F(StoreGlobalIC_Miss, 4, 1)                \
  F(StoreGlobalIC_Slow, 5, 1)                \
  F(StoreIC_Miss, 5, 1)                      \
  F(StoreInArrayLiteralIC_Slow, 5, 1)        \
  F(StorePropertyWithInterceptor, 5, 1)      \
  F(CloneObjectIC_Miss, 4, 1)                \
  F(CloneObjectIC_Slow, 2, 1)

#define FOR_EACH_INTRINSIC_RETURN_OBJECT(F) \
  FOR_EACH_INTRINSIC_ARRAY(F)               \
  FOR_EACH_INTRINSIC_ATOMICS(F)             \
  FOR_EACH_INTRINSIC_BIGINT(F)              \
  FOR_EACH_INTRINSIC_CLASSES(F)             \
  FOR_EACH_INTRINSIC_COLLECTIONS(F)         \
  FOR_EACH_INTRINSIC_COMPILER(F)            \
  FOR_EACH_INTRINSIC_DATE(F)                \
  FOR_EACH_INTRINSIC_DEBUG(F)               \
  FOR_EACH_INTRINSIC_FORIN(F)               \
  FOR_EACH_INTRINSIC_FUNCTION(F)            \
  FOR_EACH_INTRINSIC_GENERATOR(F)           \
  FOR_EACH_INTRINSIC_IC(F)                  \
  FOR_EACH_INTRINSIC_INTERNAL(F)            \
  FOR_EACH_INTRINSIC_INTERPRETER(F)         \
  FOR_EACH_INTRINSIC_INTL(F)                \
  FOR_EACH_INTRINSIC_LITERALS(F)            \
  FOR_EACH_INTRINSIC_MATHS(F)               \
  FOR_EACH_INTRINSIC_MODULE(F)              \
  FOR_EACH_INTRINSIC_NUMBERS(F)             \
  FOR_EACH_INTRINSIC_OBJECT(F)              \
  FOR_EACH_INTRINSIC_OPERATORS(F)           \
  FOR_EACH_INTRINSIC_PROMISE(F)             \
  FOR_EACH_INTRINSIC_PROXY(F)               \
  FOR_EACH_INTRINSIC_REGEXP(F)              \
  FOR_EACH_INTRINSIC_SCOPES(F)              \
  FOR_EACH_INTRINSIC_STRINGS(F)             \
  FOR_EACH_INTRINSIC_SYMBOL(F)              \
  FOR_EACH_INTRINSIC_TEST(F)                \
  FOR_EACH_INTRINSIC_TYPEDARRAY(F)          \
  FOR_EACH_INTRINSIC_WASM(F)

// FOR_EACH_INTRINSIC defines the list of all intrinsics, coming in 2 flavors,
// either returning an object or a pair.
#define FOR_EACH_INTRINSIC(F)         \
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

  // Checks whether the runtime function with the given {id} never returns
  // to it's caller normally, i.e. whether it'll always raise an exception.
  // More specifically: The C++ implementation returns the Heap::exception
  // sentinel, always.
  static bool IsNonReturning(FunctionId id);

  // Get the intrinsic function with the given name.
  static const Function* FunctionForName(const unsigned char* name, int length);

  // Get the intrinsic function with the given FunctionId.
  V8_EXPORT_PRIVATE static const Function* FunctionForId(FunctionId id);

  // Get the intrinsic function with the given function entry address.
  static const Function* FunctionForEntry(Address ref);

  // Get the runtime intrinsic function table.
  static const Function* RuntimeFunctionTable(Isolate* isolate);

  V8_WARN_UNUSED_RESULT static Maybe<bool> DeleteObjectProperty(
      Isolate* isolate, Handle<JSReceiver> receiver, Handle<Object> key,
      LanguageMode language_mode);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> SetObjectProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key,
      Handle<Object> value, LanguageMode language_mode);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> GetObjectProperty(
      Isolate* isolate, Handle<Object> object, Handle<Object> key,
      bool* is_found_out = nullptr);

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSArray> GetInternalProperties(
      Isolate* isolate, Handle<Object>);

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> ThrowIteratorError(
      Isolate* isolate, Handle<Object> object);
};


class RuntimeState {
 public:
#ifndef V8_INTL_SUPPORT
  unibrow::Mapping<unibrow::ToUppercase, 128>* to_upper_mapping() {
    return &to_upper_mapping_;
  }
  unibrow::Mapping<unibrow::ToLowercase, 128>* to_lower_mapping() {
    return &to_lower_mapping_;
  }
#endif

  Runtime::Function* redirected_intrinsic_functions() {
    return redirected_intrinsic_functions_.get();
  }

  void set_redirected_intrinsic_functions(
      Runtime::Function* redirected_intrinsic_functions) {
    redirected_intrinsic_functions_.reset(redirected_intrinsic_functions);
  }

 private:
  RuntimeState() {}
#ifndef V8_INTL_SUPPORT
  unibrow::Mapping<unibrow::ToUppercase, 128> to_upper_mapping_;
  unibrow::Mapping<unibrow::ToLowercase, 128> to_lower_mapping_;
#endif

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

// A set of bits returned by Runtime_GetOptimizationStatus.
// These bits must be in sync with bits defined in test/mjsunit/mjsunit.js
enum class OptimizationStatus {
  kIsFunction = 1 << 0,
  kNeverOptimize = 1 << 1,
  kAlwaysOptimize = 1 << 2,
  kMaybeDeopted = 1 << 3,
  kOptimized = 1 << 4,
  kTurboFanned = 1 << 5,
  kInterpreted = 1 << 6,
  kMarkedForOptimization = 1 << 7,
  kMarkedForConcurrentOptimization = 1 << 8,
  kOptimizingConcurrently = 1 << 9,
  kIsExecuting = 1 << 10,
  kTopmostFrameIsTurboFanned = 1 << 11,
};

}  // namespace internal
}  // namespace v8

#endif  // V8_RUNTIME_RUNTIME_H_
