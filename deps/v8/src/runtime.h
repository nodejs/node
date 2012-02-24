// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_RUNTIME_H_
#define V8_RUNTIME_H_

#include "allocation.h"
#include "zone.h"

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
#define RUNTIME_FUNCTION_LIST_ALWAYS_1(F) \
  /* Property access */ \
  F(GetProperty, 2, 1) \
  F(KeyedGetProperty, 2, 1) \
  F(DeleteProperty, 3, 1) \
  F(HasLocalProperty, 2, 1) \
  F(HasProperty, 2, 1) \
  F(HasElement, 2, 1) \
  F(IsPropertyEnumerable, 2, 1) \
  F(GetPropertyNames, 1, 1) \
  F(GetPropertyNamesFast, 1, 1) \
  F(GetLocalPropertyNames, 1, 1) \
  F(GetLocalElementNames, 1, 1) \
  F(GetInterceptorInfo, 1, 1) \
  F(GetNamedInterceptorPropertyNames, 1, 1) \
  F(GetIndexedInterceptorElementNames, 1, 1) \
  F(GetArgumentsProperty, 1, 1) \
  F(ToFastProperties, 1, 1) \
  F(ToSlowProperties, 1, 1) \
  F(FinishArrayPrototypeSetup, 1, 1) \
  F(SpecialArrayFunctions, 1, 1) \
  F(GetDefaultReceiver, 1, 1) \
  \
  F(GetPrototype, 1, 1) \
  F(IsInPrototypeChain, 2, 1) \
  \
  F(IsConstructCall, 0, 1) \
  \
  F(GetOwnProperty, 2, 1) \
  \
  F(IsExtensible, 1, 1) \
  F(PreventExtensions, 1, 1)\
  \
  /* Utilities */ \
  F(CheckIsBootstrapping, 0, 1) \
  F(Call, -1 /* >= 2 */, 1) \
  F(Apply, 5, 1) \
  F(GetFunctionDelegate, 1, 1) \
  F(GetConstructorDelegate, 1, 1) \
  F(NewArgumentsFast, 3, 1) \
  F(NewStrictArgumentsFast, 3, 1) \
  F(LazyCompile, 1, 1) \
  F(LazyRecompile, 1, 1) \
  F(NotifyDeoptimized, 1, 1) \
  F(NotifyOSR, 0, 1) \
  F(DeoptimizeFunction, 1, 1) \
  F(RunningInSimulator, 0, 1) \
  F(OptimizeFunctionOnNextCall, -1, 1) \
  F(GetOptimizationStatus, 1, 1) \
  F(GetOptimizationCount, 1, 1) \
  F(CompileForOnStackReplacement, 1, 1) \
  F(SetNewFunctionAttributes, 1, 1) \
  F(AllocateInNewSpace, 1, 1) \
  F(SetNativeFlag, 1, 1) \
  F(StoreArrayLiteralElement, 5, 1) \
  \
  /* Array join support */ \
  F(PushIfAbsent, 2, 1) \
  F(ArrayConcat, 1, 1) \
  \
  /* Conversions */ \
  F(ToBool, 1, 1) \
  F(Typeof, 1, 1) \
  \
  F(StringToNumber, 1, 1) \
  F(StringFromCharCodeArray, 1, 1) \
  F(StringParseInt, 2, 1) \
  F(StringParseFloat, 1, 1) \
  F(StringToLowerCase, 1, 1) \
  F(StringToUpperCase, 1, 1) \
  F(StringSplit, 3, 1) \
  F(CharFromCode, 1, 1) \
  F(URIEscape, 1, 1) \
  F(URIUnescape, 1, 1) \
  F(QuoteJSONString, 1, 1) \
  F(QuoteJSONStringComma, 1, 1) \
  F(QuoteJSONStringArray, 1, 1) \
  \
  F(NumberToString, 1, 1) \
  F(NumberToStringSkipCache, 1, 1) \
  F(NumberToInteger, 1, 1) \
  F(NumberToIntegerMapMinusZero, 1, 1) \
  F(NumberToJSUint32, 1, 1) \
  F(NumberToJSInt32, 1, 1) \
  F(NumberToSmi, 1, 1) \
  F(AllocateHeapNumber, 0, 1) \
  \
  /* Arithmetic operations */ \
  F(NumberAdd, 2, 1) \
  F(NumberSub, 2, 1) \
  F(NumberMul, 2, 1) \
  F(NumberDiv, 2, 1) \
  F(NumberMod, 2, 1) \
  F(NumberUnaryMinus, 1, 1) \
  F(NumberAlloc, 0, 1) \
  \
  F(StringAdd, 2, 1) \
  F(StringBuilderConcat, 3, 1) \
  F(StringBuilderJoin, 3, 1) \
  F(SparseJoinWithSeparator, 3, 1) \
  \
  /* Bit operations */ \
  F(NumberOr, 2, 1) \
  F(NumberAnd, 2, 1) \
  F(NumberXor, 2, 1) \
  F(NumberNot, 1, 1) \
  \
  F(NumberShl, 2, 1) \
  F(NumberShr, 2, 1) \
  F(NumberSar, 2, 1) \
  \
  /* Comparisons */ \
  F(NumberEquals, 2, 1) \
  F(StringEquals, 2, 1) \
  \
  F(NumberCompare, 3, 1) \
  F(SmiLexicographicCompare, 2, 1) \
  F(StringCompare, 2, 1) \
  \
  /* Math */ \
  F(Math_acos, 1, 1) \
  F(Math_asin, 1, 1) \
  F(Math_atan, 1, 1) \
  F(Math_atan2, 2, 1) \
  F(Math_ceil, 1, 1) \
  F(Math_cos, 1, 1) \
  F(Math_exp, 1, 1) \
  F(Math_floor, 1, 1) \
  F(Math_log, 1, 1) \
  F(Math_pow, 2, 1) \
  F(Math_pow_cfunction, 2, 1) \
  F(RoundNumber, 1, 1) \
  F(Math_sin, 1, 1) \
  F(Math_sqrt, 1, 1) \
  F(Math_tan, 1, 1) \
  \
  /* Regular expressions */ \
  F(RegExpCompile, 3, 1) \
  F(RegExpExec, 4, 1) \
  F(RegExpExecMultiple, 4, 1) \
  F(RegExpInitializeObject, 5, 1) \
  F(RegExpConstructResult, 3, 1) \
  \
  /* JSON */ \
  F(ParseJson, 1, 1) \
  \
  /* Strings */ \
  F(StringCharCodeAt, 2, 1) \
  F(StringIndexOf, 3, 1) \
  F(StringLastIndexOf, 3, 1) \
  F(StringLocaleCompare, 2, 1) \
  F(SubString, 3, 1) \
  F(StringReplaceRegExpWithString, 4, 1) \
  F(StringReplaceOneCharWithString, 3, 1) \
  F(StringMatch, 3, 1) \
  F(StringTrim, 3, 1) \
  F(StringToArray, 2, 1) \
  F(NewStringWrapper, 1, 1) \
  \
  /* Numbers */ \
  F(NumberToRadixString, 2, 1) \
  F(NumberToFixed, 2, 1) \
  F(NumberToExponential, 2, 1) \
  F(NumberToPrecision, 2, 1)

#define RUNTIME_FUNCTION_LIST_ALWAYS_2(F) \
  /* Reflection */ \
  F(FunctionSetInstanceClassName, 2, 1) \
  F(FunctionSetLength, 2, 1) \
  F(FunctionSetPrototype, 2, 1) \
  F(FunctionSetReadOnlyPrototype, 1, 1) \
  F(FunctionGetName, 1, 1) \
  F(FunctionSetName, 2, 1) \
  F(FunctionNameShouldPrintAsAnonymous, 1, 1) \
  F(FunctionMarkNameShouldPrintAsAnonymous, 1, 1) \
  F(FunctionBindArguments, 4, 1) \
  F(BoundFunctionGetBindings, 1, 1) \
  F(FunctionRemovePrototype, 1, 1) \
  F(FunctionGetSourceCode, 1, 1) \
  F(FunctionGetScript, 1, 1) \
  F(FunctionGetScriptSourcePosition, 1, 1) \
  F(FunctionGetPositionForOffset, 2, 1) \
  F(FunctionIsAPIFunction, 1, 1) \
  F(FunctionIsBuiltin, 1, 1) \
  F(GetScript, 1, 1) \
  F(CollectStackTrace, 3, 1) \
  F(GetV8Version, 0, 1) \
  \
  F(ClassOf, 1, 1) \
  F(SetCode, 2, 1) \
  F(SetExpectedNumberOfProperties, 2, 1) \
  \
  F(CreateApiFunction, 1, 1) \
  F(IsTemplate, 1, 1) \
  F(GetTemplateField, 2, 1) \
  F(DisableAccessChecks, 1, 1) \
  F(EnableAccessChecks, 1, 1) \
  \
  /* Dates */ \
  F(DateCurrentTime, 0, 1) \
  F(DateParseString, 2, 1) \
  F(DateLocalTimezone, 1, 1) \
  F(DateLocalTimeOffset, 0, 1) \
  F(DateDaylightSavingsOffset, 1, 1) \
  F(DateMakeDay, 2, 1) \
  F(DateYMDFromTime, 2, 1) \
  \
  /* Numbers */ \
  \
  /* Globals */ \
  F(CompileString, 1, 1) \
  F(GlobalPrint, 1, 1) \
  \
  /* Eval */ \
  F(GlobalReceiver, 1, 1) \
  F(ResolvePossiblyDirectEval, 5, 2) \
  \
  F(SetProperty, -1 /* 4 or 5 */, 1) \
  F(DefineOrRedefineDataProperty, 4, 1) \
  F(DefineOrRedefineAccessorProperty, 5, 1) \
  F(IgnoreAttributesAndSetProperty, -1 /* 3 or 4 */, 1) \
  \
  /* Arrays */ \
  F(RemoveArrayHoles, 2, 1) \
  F(GetArrayKeys, 2, 1) \
  F(MoveArrayContents, 2, 1) \
  F(EstimateNumberOfElements, 1, 1) \
  F(SwapElements, 3, 1) \
  \
  /* Getters and Setters */ \
  F(LookupAccessor, 3, 1) \
  \
  /* Literals */ \
  F(MaterializeRegExpLiteral, 4, 1)\
  F(CreateObjectLiteral, 4, 1) \
  F(CreateObjectLiteralShallow, 4, 1) \
  F(CreateArrayLiteral, 3, 1) \
  F(CreateArrayLiteralShallow, 3, 1) \
  \
  /* Harmony proxies */ \
  F(CreateJSProxy, 2, 1) \
  F(CreateJSFunctionProxy, 4, 1) \
  F(IsJSProxy, 1, 1) \
  F(IsJSFunctionProxy, 1, 1) \
  F(GetHandler, 1, 1) \
  F(GetCallTrap, 1, 1) \
  F(GetConstructTrap, 1, 1) \
  F(Fix, 1, 1) \
  \
  /* Harmony sets */ \
  F(SetInitialize, 1, 1) \
  F(SetAdd, 2, 1) \
  F(SetHas, 2, 1) \
  F(SetDelete, 2, 1) \
  \
  /* Harmony maps */ \
  F(MapInitialize, 1, 1) \
  F(MapGet, 2, 1) \
  F(MapSet, 3, 1) \
  \
  /* Harmony weakmaps */ \
  F(WeakMapInitialize, 1, 1) \
  F(WeakMapGet, 2, 1) \
  F(WeakMapSet, 3, 1) \
  \
  /* Statements */ \
  F(NewClosure, 3, 1) \
  F(NewObject, 1, 1) \
  F(NewObjectFromBound, 1, 1) \
  F(FinalizeInstanceSize, 1, 1) \
  F(Throw, 1, 1) \
  F(ReThrow, 1, 1) \
  F(ThrowReferenceError, 1, 1) \
  F(StackGuard, 0, 1) \
  F(Interrupt, 0, 1) \
  F(PromoteScheduledException, 0, 1) \
  \
  /* Contexts */ \
  F(NewFunctionContext, 1, 1) \
  F(PushWithContext, 2, 1) \
  F(PushCatchContext, 3, 1) \
  F(PushBlockContext, 2, 1) \
  F(DeleteContextSlot, 2, 1) \
  F(LoadContextSlot, 2, 2) \
  F(LoadContextSlotNoReferenceError, 2, 2) \
  F(StoreContextSlot, 4, 1) \
  \
  /* Declarations and initialization */ \
  F(DeclareGlobals, 3, 1) \
  F(DeclareContextSlot, 4, 1) \
  F(InitializeVarGlobal, -1 /* 2 or 3 */, 1) \
  F(InitializeConstGlobal, 2, 1) \
  F(InitializeConstContextSlot, 3, 1) \
  F(OptimizeObjectForAddingMultipleProperties, 2, 1) \
  \
  /* Debugging */ \
  F(DebugPrint, 1, 1) \
  F(DebugTrace, 0, 1) \
  F(TraceEnter, 0, 1) \
  F(TraceExit, 1, 1) \
  F(Abort, 2, 1) \
  /* Logging */ \
  F(Log, 2, 1) \
  /* ES5 */ \
  F(LocalKeys, 1, 1) \
  /* Cache suport */ \
  F(GetFromCache, 2, 1) \
  \
  /* Message objects */ \
  F(NewMessageObject, 2, 1) \
  F(MessageGetType, 1, 1) \
  F(MessageGetArguments, 1, 1) \
  F(MessageGetStartPosition, 1, 1) \
  F(MessageGetScript, 1, 1) \
  \
  /* Pseudo functions - handled as macros by parser */ \
  F(IS_VAR, 1, 1) \
  \
  /* expose boolean functions from objects-inl.h */ \
  F(HasFastSmiOnlyElements, 1, 1) \
  F(HasFastElements, 1, 1) \
  F(HasFastDoubleElements, 1, 1) \
  F(HasDictionaryElements, 1, 1) \
  F(HasExternalPixelElements, 1, 1) \
  F(HasExternalArrayElements, 1, 1) \
  F(HasExternalByteElements, 1, 1) \
  F(HasExternalUnsignedByteElements, 1, 1) \
  F(HasExternalShortElements, 1, 1) \
  F(HasExternalUnsignedShortElements, 1, 1) \
  F(HasExternalIntElements, 1, 1) \
  F(HasExternalUnsignedIntElements, 1, 1) \
  F(HasExternalFloatElements, 1, 1) \
  F(HasExternalDoubleElements, 1, 1) \
  F(TransitionElementsSmiToDouble, 1, 1) \
  F(TransitionElementsDoubleToObject, 1, 1) \
  F(HaveSameMap, 2, 1) \
  /* profiler */ \
  F(ProfilerResume, 0, 1) \
  F(ProfilerPause, 0, 1)


#ifdef ENABLE_DEBUGGER_SUPPORT
#define RUNTIME_FUNCTION_LIST_DEBUGGER_SUPPORT(F) \
  /* Debugger support*/ \
  F(DebugBreak, 0, 1) \
  F(SetDebugEventListener, 2, 1) \
  F(Break, 0, 1) \
  F(DebugGetPropertyDetails, 2, 1) \
  F(DebugGetProperty, 2, 1) \
  F(DebugPropertyTypeFromDetails, 1, 1) \
  F(DebugPropertyAttributesFromDetails, 1, 1) \
  F(DebugPropertyIndexFromDetails, 1, 1) \
  F(DebugNamedInterceptorPropertyValue, 2, 1) \
  F(DebugIndexedInterceptorElementValue, 2, 1) \
  F(CheckExecutionState, 1, 1) \
  F(GetFrameCount, 1, 1) \
  F(GetFrameDetails, 2, 1) \
  F(GetScopeCount, 2, 1) \
  F(GetScopeDetails, 4, 1) \
  F(DebugPrintScopes, 0, 1) \
  F(GetThreadCount, 1, 1) \
  F(GetThreadDetails, 2, 1) \
  F(SetDisableBreak, 1, 1) \
  F(GetBreakLocations, 1, 1) \
  F(SetFunctionBreakPoint, 3, 1) \
  F(SetScriptBreakPoint, 3, 1) \
  F(ClearBreakPoint, 1, 1) \
  F(ChangeBreakOnException, 2, 1) \
  F(IsBreakOnException, 1, 1) \
  F(PrepareStep, 3, 1) \
  F(ClearStepping, 0, 1) \
  F(DebugEvaluate, 6, 1) \
  F(DebugEvaluateGlobal, 4, 1) \
  F(DebugGetLoadedScripts, 0, 1) \
  F(DebugReferencedBy, 3, 1) \
  F(DebugConstructedBy, 2, 1) \
  F(DebugGetPrototype, 1, 1) \
  F(SystemBreak, 0, 1) \
  F(DebugDisassembleFunction, 1, 1) \
  F(DebugDisassembleConstructor, 1, 1) \
  F(FunctionGetInferredName, 1, 1) \
  F(LiveEditFindSharedFunctionInfosForScript, 1, 1) \
  F(LiveEditGatherCompileInfo, 2, 1) \
  F(LiveEditReplaceScript, 3, 1) \
  F(LiveEditReplaceFunctionCode, 2, 1) \
  F(LiveEditFunctionSourceUpdated, 1, 1) \
  F(LiveEditFunctionSetScript, 2, 1) \
  F(LiveEditReplaceRefToNestedFunction, 3, 1) \
  F(LiveEditPatchFunctionPositions, 2, 1) \
  F(LiveEditCheckAndDropActivations, 2, 1) \
  F(LiveEditCompareStrings, 2, 1) \
  F(GetFunctionCodePositionFromSource, 2, 1) \
  F(ExecuteInDebugContext, 2, 1) \
  \
  F(SetFlags, 1, 1) \
  F(CollectGarbage, 1, 1) \
  F(GetHeapUsage, 0, 1) \
  \
  /* LiveObjectList support*/ \
  F(HasLOLEnabled, 0, 1) \
  F(CaptureLOL, 0, 1) \
  F(DeleteLOL, 1, 1) \
  F(DumpLOL, 5, 1) \
  F(GetLOLObj, 1, 1) \
  F(GetLOLObjId, 1, 1) \
  F(GetLOLObjRetainers, 6, 1) \
  F(GetLOLPath, 3, 1) \
  F(InfoLOL, 2, 1) \
  F(PrintLOLObj, 1, 1) \
  F(ResetLOL, 0, 1) \
  F(SummarizeLOL, 3, 1)

#else
#define RUNTIME_FUNCTION_LIST_DEBUGGER_SUPPORT(F)
#endif

#ifdef DEBUG
#define RUNTIME_FUNCTION_LIST_DEBUG(F) \
  /* Testing */ \
  F(ListNatives, 0, 1)
#else
#define RUNTIME_FUNCTION_LIST_DEBUG(F)
#endif

// ----------------------------------------------------------------------------
// RUNTIME_FUNCTION_LIST defines all runtime functions accessed
// either directly by id (via the code generator), or indirectly
// via a native call by name (from within JS code).

#define RUNTIME_FUNCTION_LIST(F) \
  RUNTIME_FUNCTION_LIST_ALWAYS_1(F) \
  RUNTIME_FUNCTION_LIST_ALWAYS_2(F) \
  RUNTIME_FUNCTION_LIST_DEBUG(F) \
  RUNTIME_FUNCTION_LIST_DEBUGGER_SUPPORT(F)

// ----------------------------------------------------------------------------
// INLINE_FUNCTION_LIST defines all inlined functions accessed
// with a native call of the form %_name from within JS code.
// Entries have the form F(name, number of arguments, number of return values).
#define INLINE_FUNCTION_LIST(F) \
  F(IsSmi, 1, 1)                                                             \
  F(IsNonNegativeSmi, 1, 1)                                                  \
  F(IsArray, 1, 1)                                                           \
  F(IsRegExp, 1, 1)                                                          \
  F(CallFunction, -1 /* receiver + n args + function */, 1)                  \
  F(ArgumentsLength, 0, 1)                                                   \
  F(Arguments, 1, 1)                                                         \
  F(ValueOf, 1, 1)                                                           \
  F(SetValueOf, 2, 1)                                                        \
  F(StringCharFromCode, 1, 1)                                                \
  F(StringCharAt, 2, 1)                                                      \
  F(ObjectEquals, 2, 1)                                                      \
  F(RandomHeapNumber, 0, 1)                                                  \
  F(IsObject, 1, 1)                                                          \
  F(IsFunction, 1, 1)                                                        \
  F(IsUndetectableObject, 1, 1)                                              \
  F(IsSpecObject, 1, 1)                                                      \
  F(IsStringWrapperSafeForDefaultValueOf, 1, 1)                              \
  F(MathPow, 2, 1)                                                           \
  F(MathSin, 1, 1)                                                           \
  F(MathCos, 1, 1)                                                           \
  F(MathTan, 1, 1)                                                           \
  F(MathSqrt, 1, 1)                                                          \
  F(MathLog, 1, 1)                                                           \
  F(IsRegExpEquivalent, 2, 1)                                                \
  F(HasCachedArrayIndex, 1, 1)                                               \
  F(GetCachedArrayIndex, 1, 1)                                               \
  F(FastAsciiArrayJoin, 2, 1)


// ----------------------------------------------------------------------------
// INLINE_AND_RUNTIME_FUNCTION_LIST defines all inlined functions accessed
// with a native call of the form %_name from within JS code that also have
// a corresponding runtime function, that is called for slow cases.
// Entries have the form F(name, number of arguments, number of return values).
#define INLINE_RUNTIME_FUNCTION_LIST(F) \
  F(IsConstructCall, 0, 1)                                                   \
  F(ClassOf, 1, 1)                                                           \
  F(StringCharCodeAt, 2, 1)                                                  \
  F(Log, 3, 1)                                                               \
  F(StringAdd, 2, 1)                                                         \
  F(SubString, 3, 1)                                                         \
  F(StringCompare, 2, 1)                                                     \
  F(RegExpExec, 4, 1)                                                        \
  F(RegExpConstructResult, 3, 1)                                             \
  F(GetFromCache, 2, 1)                                                      \
  F(NumberToString, 1, 1)                                                    \
  F(SwapElements, 3, 1)


//---------------------------------------------------------------------------
// Runtime provides access to all C++ runtime functions.

class RuntimeState {
 public:
  StaticResource<StringInputBuffer>* string_input_buffer() {
    return &string_input_buffer_;
  }
  unibrow::Mapping<unibrow::ToUppercase, 128>* to_upper_mapping() {
    return &to_upper_mapping_;
  }
  unibrow::Mapping<unibrow::ToLowercase, 128>* to_lower_mapping() {
    return &to_lower_mapping_;
  }
  StringInputBuffer* string_input_buffer_compare_bufx() {
    return &string_input_buffer_compare_bufx_;
  }
  StringInputBuffer* string_input_buffer_compare_bufy() {
    return &string_input_buffer_compare_bufy_;
  }
  StringInputBuffer* string_locale_compare_buf1() {
    return &string_locale_compare_buf1_;
  }
  StringInputBuffer* string_locale_compare_buf2() {
    return &string_locale_compare_buf2_;
  }

 private:
  RuntimeState() {}
  // Non-reentrant string buffer for efficient general use in the runtime.
  StaticResource<StringInputBuffer> string_input_buffer_;
  unibrow::Mapping<unibrow::ToUppercase, 128> to_upper_mapping_;
  unibrow::Mapping<unibrow::ToLowercase, 128> to_lower_mapping_;
  StringInputBuffer string_input_buffer_compare_bufx_;
  StringInputBuffer string_input_buffer_compare_bufy_;
  StringInputBuffer string_locale_compare_buf1_;
  StringInputBuffer string_locale_compare_buf2_;

  friend class Isolate;
  friend class Runtime;

  DISALLOW_COPY_AND_ASSIGN(RuntimeState);
};


class Runtime : public AllStatic {
 public:
  enum FunctionId {
#define F(name, nargs, ressize) k##name,
    RUNTIME_FUNCTION_LIST(F)
#undef F
#define F(name, nargs, ressize) kInline##name,
    INLINE_FUNCTION_LIST(F)
    INLINE_RUNTIME_FUNCTION_LIST(F)
#undef F
    kNumFunctions,
    kFirstInlineFunction = kInlineIsSmi
  };

  enum IntrinsicType {
    RUNTIME,
    INLINE
  };

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

  // Add symbols for all the intrinsic function names to a StringDictionary.
  // Returns failure if an allocation fails.  In this case, it must be
  // retried with a new, empty StringDictionary, not with the same one.
  // Alternatively, heap initialization can be completely restarted.
  MUST_USE_RESULT static MaybeObject* InitializeIntrinsicFunctionNames(
      Heap* heap, Object* dictionary);

  // Get the intrinsic function with the given name, which must be a symbol.
  static const Function* FunctionForSymbol(Handle<String> name);

  // Get the intrinsic function with the given FunctionId.
  static const Function* FunctionForId(FunctionId id);

  static Handle<String> StringReplaceOneCharWithString(Isolate* isolate,
                                                       Handle<String> subject,
                                                       Handle<String> search,
                                                       Handle<String> replace,
                                                       bool* found,
                                                       int recursion_limit);

  // General-purpose helper functions for runtime system.
  static int StringMatch(Isolate* isolate,
                         Handle<String> sub,
                         Handle<String> pat,
                         int index);

  static bool IsUpperCaseChar(RuntimeState* runtime_state, uint16_t ch);

  // TODO(1240886): Some of the following methods are *not* handle safe, but
  // accept handle arguments. This seems fragile.

  // Support getting the characters in a string using [] notation as
  // in Firefox/SpiderMonkey, Safari and Opera.
  MUST_USE_RESULT static MaybeObject* GetElementOrCharAt(Isolate* isolate,
                                                         Handle<Object> object,
                                                         uint32_t index);

  MUST_USE_RESULT static MaybeObject* SetObjectProperty(
      Isolate* isolate,
      Handle<Object> object,
      Handle<Object> key,
      Handle<Object> value,
      PropertyAttributes attr,
      StrictModeFlag strict_mode);

  MUST_USE_RESULT static MaybeObject* ForceSetObjectProperty(
      Isolate* isolate,
      Handle<JSObject> object,
      Handle<Object> key,
      Handle<Object> value,
      PropertyAttributes attr);

  MUST_USE_RESULT static MaybeObject* ForceDeleteObjectProperty(
      Isolate* isolate,
      Handle<JSReceiver> object,
      Handle<Object> key);

  MUST_USE_RESULT static MaybeObject* GetObjectProperty(
      Isolate* isolate,
      Handle<Object> object,
      Handle<Object> key);

  // This function is used in FunctionNameUsing* tests.
  static Object* FindSharedFunctionInfoInScript(Isolate* isolate,
                                                Handle<Script> script,
                                                int position);

  // Helper functions used stubs.
  static void PerformGC(Object* result);

  // Used in runtime.cc and hydrogen's VisitArrayLiteral.
  static Handle<Object> CreateArrayLiteralBoilerplate(
      Isolate* isolate,
      Handle<FixedArray> literals,
      Handle<FixedArray> elements);
};


//---------------------------------------------------------------------------
// Constants used by interface to runtime functions.

class DeclareGlobalsEvalFlag:     public BitField<bool,         0, 1> {};
class DeclareGlobalsNativeFlag:   public BitField<bool,         1, 1> {};
class DeclareGlobalsLanguageMode: public BitField<LanguageMode, 2, 2> {};

} }  // namespace v8::internal

#endif  // V8_RUNTIME_H_
