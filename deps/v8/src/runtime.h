// Copyright 2006-2008 the V8 project authors. All rights reserved.
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
  F(DeleteProperty, 2, 1) \
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
  \
  F(IsInPrototypeChain, 2, 1) \
  F(SetHiddenPrototype, 2, 1) \
  \
  F(IsConstructCall, 0, 1) \
  \
  F(GetOwnProperty, 2, 1) \
  \
  F(IsExtensible, 1, 1) \
  \
  /* Utilities */ \
  F(GetFunctionDelegate, 1, 1) \
  F(GetConstructorDelegate, 1, 1) \
  F(NewArgumentsFast, 3, 1) \
  F(LazyCompile, 1, 1) \
  F(SetNewFunctionAttributes, 1, 1) \
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
  \
  F(NumberToString, 1, 1) \
  F(NumberToInteger, 1, 1) \
  F(NumberToJSUint32, 1, 1) \
  F(NumberToJSInt32, 1, 1) \
  F(NumberToSmi, 1, 1) \
  \
  /* Arithmetic operations */ \
  F(NumberAdd, 2, 1) \
  F(NumberSub, 2, 1) \
  F(NumberMul, 2, 1) \
  F(NumberDiv, 2, 1) \
  F(NumberMod, 2, 1) \
  F(NumberUnaryMinus, 1, 1) \
  \
  F(StringAdd, 2, 1) \
  F(StringBuilderConcat, 3, 1) \
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
  \
  /* Strings */ \
  F(StringCharCodeAt, 2, 1) \
  F(StringCharAt, 2, 1) \
  F(StringIndexOf, 3, 1) \
  F(StringLastIndexOf, 3, 1) \
  F(StringLocaleCompare, 2, 1) \
  F(SubString, 3, 1) \
  F(StringReplaceRegExpWithString, 4, 1) \
  F(StringMatch, 3, 1) \
  F(StringTrim, 3, 1) \
  F(StringToArray, 1, 1) \
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
  F(FunctionGetName, 1, 1) \
  F(FunctionSetName, 2, 1) \
  F(FunctionGetSourceCode, 1, 1) \
  F(FunctionGetScript, 1, 1) \
  F(FunctionGetScriptSourcePosition, 1, 1) \
  F(FunctionGetPositionForOffset, 2, 1) \
  F(FunctionIsAPIFunction, 1, 1) \
  F(FunctionIsBuiltin, 1, 1) \
  F(GetScript, 1, 1) \
  F(CollectStackTrace, 2, 1) \
  F(GetV8Version, 0, 1) \
  \
  F(ClassOf, 1, 1) \
  F(SetCode, 2, 1) \
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
  F(DateMakeDay, 3, 1) \
  F(DateYMDFromTime, 2, 1) \
  \
  /* Numbers */ \
  F(NumberIsFinite, 1, 1) \
  \
  /* Globals */ \
  F(CompileString, 2, 1) \
  F(GlobalPrint, 1, 1) \
  \
  /* Eval */ \
  F(GlobalReceiver, 1, 1) \
  F(ResolvePossiblyDirectEval, 3, 2) \
  \
  F(SetProperty, -1 /* 3 or 4 */, 1) \
  F(DefineOrRedefineDataProperty, 4, 1) \
  F(DefineOrRedefineAccessorProperty, 5, 1) \
  F(IgnoreAttributesAndSetProperty, -1 /* 3 or 4 */, 1) \
  \
  /* Arrays */ \
  F(RemoveArrayHoles, 2, 1) \
  F(GetArrayKeys, 2, 1) \
  F(MoveArrayContents, 2, 1) \
  F(EstimateNumberOfElements, 1, 1) \
  \
  /* Getters and Setters */ \
  F(DefineAccessor, -1 /* 4 or 5 */, 1) \
  F(LookupAccessor, 3, 1) \
  \
  /* Literals */ \
  F(MaterializeRegExpLiteral, 4, 1)\
  F(CreateArrayLiteralBoilerplate, 3, 1) \
  F(CloneLiteralBoilerplate, 1, 1) \
  F(CloneShallowLiteralBoilerplate, 1, 1) \
  F(CreateObjectLiteral, 4, 1) \
  F(CreateObjectLiteralShallow, 4, 1) \
  F(CreateArrayLiteral, 3, 1) \
  F(CreateArrayLiteralShallow, 3, 1) \
  \
  /* Catch context extension objects */ \
  F(CreateCatchExtensionObject, 2, 1) \
  \
  /* Statements */ \
  F(NewClosure, 2, 1) \
  F(NewObject, 1, 1) \
  F(Throw, 1, 1) \
  F(ReThrow, 1, 1) \
  F(ThrowReferenceError, 1, 1) \
  F(StackGuard, 1, 1) \
  F(PromoteScheduledException, 0, 1) \
  \
  /* Contexts */ \
  F(NewContext, 1, 1) \
  F(PushContext, 1, 1) \
  F(PushCatchContext, 1, 1) \
  F(LookupContext, 2, 1) \
  F(LoadContextSlot, 2, 2) \
  F(LoadContextSlotNoReferenceError, 2, 2) \
  F(StoreContextSlot, 3, 1) \
  \
  /* Declarations and initialization */ \
  F(DeclareGlobals, 3, 1) \
  F(DeclareContextSlot, 4, 1) \
  F(InitializeVarGlobal, -1 /* 1 or 2 */, 1) \
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
  /* Handle scopes */ \
  F(DeleteHandleScopeExtensions, 0, 1) \
  \
  /* Pseudo functions - handled as macros by parser */ \
  F(IS_VAR, 1, 1)

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
  F(GetScopeDetails, 3, 1) \
  F(DebugPrintScopes, 0, 1) \
  F(GetCFrames, 1, 1) \
  F(GetThreadCount, 1, 1) \
  F(GetThreadDetails, 2, 1) \
  F(GetBreakLocations, 1, 1) \
  F(SetFunctionBreakPoint, 3, 1) \
  F(SetScriptBreakPoint, 3, 1) \
  F(ClearBreakPoint, 1, 1) \
  F(ChangeBreakOnException, 2, 1) \
  F(PrepareStep, 3, 1) \
  F(ClearStepping, 0, 1) \
  F(DebugEvaluate, 4, 1) \
  F(DebugEvaluateGlobal, 3, 1) \
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
  F(LiveEditRelinkFunctionToScript, 2, 1) \
  F(LiveEditPatchFunctionPositions, 2, 1) \
  F(LiveEditCheckStackActivations, 1, 1) \
  F(GetFunctionCodePositionFromSource, 2, 1)
#else
#define RUNTIME_FUNCTION_LIST_DEBUGGER_SUPPORT(F)
#endif

#ifdef ENABLE_LOGGING_AND_PROFILING
#define RUNTIME_FUNCTION_LIST_PROFILER_SUPPORT(F) \
  F(ProfilerResume, 2, 1) \
  F(ProfilerPause, 2, 1)
#else
#define RUNTIME_FUNCTION_LIST_PROFILER_SUPPORT(F)
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
  RUNTIME_FUNCTION_LIST_DEBUGGER_SUPPORT(F) \
  RUNTIME_FUNCTION_LIST_PROFILER_SUPPORT(F)

// ----------------------------------------------------------------------------
// Runtime provides access to all C++ runtime functions.

class Runtime : public AllStatic {
 public:
  enum FunctionId {
#define F(name, nargs, ressize) k##name,
    RUNTIME_FUNCTION_LIST(F)
    kNofFunctions
#undef F
  };

  // Runtime function descriptor.
  struct Function {
    // The JS name of the function.
    const char* name;

    // The C++ (native) entry point.
    byte* entry;

    // The number of arguments expected; nargs < 0 if variable no. of
    // arguments.
    int nargs;
    int stub_id;
    // Size of result, if complex (larger than a single pointer),
    // otherwise zero.
    int result_size;
  };

  // Get the runtime function with the given function id.
  static Function* FunctionForId(FunctionId fid);

  // Get the runtime function with the given name.
  static Function* FunctionForName(const char* name);

  static int StringMatch(Handle<String> sub, Handle<String> pat, int index);

  static bool IsUpperCaseChar(uint16_t ch);

  // TODO(1240886): The following three methods are *not* handle safe,
  // but accept handle arguments. This seems fragile.

  // Support getting the characters in a string using [] notation as
  // in Firefox/SpiderMonkey, Safari and Opera.
  static Object* GetElementOrCharAt(Handle<Object> object, uint32_t index);
  static Object* GetElement(Handle<Object> object, uint32_t index);

  static Object* SetObjectProperty(Handle<Object> object,
                                   Handle<Object> key,
                                   Handle<Object> value,
                                   PropertyAttributes attr);

  static Object* ForceSetObjectProperty(Handle<JSObject> object,
                                        Handle<Object> key,
                                        Handle<Object> value,
                                        PropertyAttributes attr);

  static Object* ForceDeleteObjectProperty(Handle<JSObject> object,
                                           Handle<Object> key);

  static Object* GetObjectProperty(Handle<Object> object, Handle<Object> key);

  // This function is used in FunctionNameUsing* tests.
  static Object* FindSharedFunctionInfoInScript(Handle<Script> script,
                                                int position);

  // Helper functions used stubs.
  static void PerformGC(Object* result);
};


} }  // namespace v8::internal

#endif  // V8_RUNTIME_H_
