// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SHARED_FUNCTION_INFO_H_
#define V8_OBJECTS_SHARED_FUNCTION_INFO_H_

#include "src/objects.h"
#include "src/objects/script.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class BytecodeArray;
class CoverageInfo;
class DebugInfo;

class PreParsedScopeData : public Struct {
 public:
  DECL_ACCESSORS(scope_data, PodArray<uint8_t>)
  DECL_ACCESSORS(child_data, FixedArray)

  static const int kScopeDataOffset = Struct::kHeaderSize;
  static const int kChildDataOffset = kScopeDataOffset + kPointerSize;
  static const int kSize = kChildDataOffset + kPointerSize;

  DECL_CAST(PreParsedScopeData)
  DECL_PRINTER(PreParsedScopeData)
  DECL_VERIFIER(PreParsedScopeData)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PreParsedScopeData);
};

class InterpreterData : public Struct {
 public:
  DECL_ACCESSORS(bytecode_array, BytecodeArray)
  DECL_ACCESSORS(interpreter_trampoline, Code)

  static const int kBytecodeArrayOffset = Struct::kHeaderSize;
  static const int kInterpreterTrampolineOffset =
      kBytecodeArrayOffset + kPointerSize;
  static const int kSize = kInterpreterTrampolineOffset + kPointerSize;

  DECL_CAST(InterpreterData)
  DECL_PRINTER(InterpreterData)
  DECL_VERIFIER(InterpreterData)

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InterpreterData);
};

// SharedFunctionInfo describes the JSFunction information that can be
// shared by multiple instances of the function.
class SharedFunctionInfo : public HeapObject {
 public:
  static constexpr Object* const kNoSharedNameSentinel = Smi::kZero;

  // [name]: Returns shared name if it exists or an empty string otherwise.
  inline String* Name() const;
  inline void SetName(String* name);

  // Get the code object which represents the execution of this function.
  inline Code* GetCode() const;

  // Get the abstract code associated with the function, which will either be
  // a Code object or a BytecodeArray.
  inline AbstractCode* abstract_code();

  // Tells whether or not this shared function info is interpreted.
  //
  // Note: function->IsInterpreted() does not necessarily return the same value
  // as function->shared()->IsInterpreted() because the closure might have been
  // optimized.
  inline bool IsInterpreted() const;

  // Set up the link between shared function info and the script. The shared
  // function info is added to the list on the script.
  V8_EXPORT_PRIVATE static void SetScript(
      Handle<SharedFunctionInfo> shared, Handle<Object> script_object,
      bool reset_preparsed_scope_data = true);

  // Layout description of the optimized code map.
  static const int kEntriesStart = 0;
  static const int kContextOffset = 0;
  static const int kCachedCodeOffset = 1;
  static const int kEntryLength = 2;
  static const int kInitialLength = kEntriesStart + kEntryLength;

  static const int kNotFound = -1;
  static const int kInvalidLength = -1;

  // Helpers for assembly code that does a backwards walk of the optimized code
  // map.
  static const int kOffsetToPreviousContext =
      FixedArray::kHeaderSize + kPointerSize * (kContextOffset - kEntryLength);
  static const int kOffsetToPreviousCachedCode =
      FixedArray::kHeaderSize +
      kPointerSize * (kCachedCodeOffset - kEntryLength);

  // [scope_info]: Scope info.
  DECL_ACCESSORS(scope_info, ScopeInfo)

  // End position of this function in the script source.
  inline int EndPosition() const;

  // Start position of this function in the script source.
  inline int StartPosition() const;

  // [outer scope info | feedback metadata] Shared storage for outer scope info
  // (on uncompiled functions) and feedback metadata (on compiled functions).
  DECL_ACCESSORS(raw_outer_scope_info_or_feedback_metadata, HeapObject)

  // Get the outer scope info whether this function is compiled or not.
  inline bool HasOuterScopeInfo() const;
  inline ScopeInfo* GetOuterScopeInfo() const;

  // [feedback metadata] Metadata template for feedback vectors of instances of
  // this function.
  inline bool HasFeedbackMetadata() const;
  DECL_ACCESSORS(feedback_metadata, FeedbackMetadata)

  // Returns if this function has been compiled to native code yet.
  inline bool is_compiled() const;

  // [length]: The function length - usually the number of declared parameters.
  // Use up to 2^30 parameters. The value is only reliable when the function has
  // been compiled.
  inline int GetLength() const;
  inline bool HasLength() const;
  inline void set_length(int value);

  // [internal formal parameter count]: The declared number of parameters.
  // For subclass constructors, also includes new.target.
  // The size of function's frame is internal_formal_parameter_count + 1.
  DECL_INT_ACCESSORS(internal_formal_parameter_count)

  // Set the formal parameter count so the function code will be
  // called without using argument adaptor frames.
  inline void DontAdaptArguments();

  // [expected_nof_properties]: Expected number of properties for the
  // function. The value is only reliable when the function has been compiled.
  DECL_INT_ACCESSORS(expected_nof_properties)

  // [function_literal_id] - uniquely identifies the FunctionLiteral this
  // SharedFunctionInfo represents within its script, or -1 if this
  // SharedFunctionInfo object doesn't correspond to a parsed FunctionLiteral.
  DECL_INT_ACCESSORS(function_literal_id)

#if V8_SFI_HAS_UNIQUE_ID
  // [unique_id] - For --trace-maps purposes, an identifier that's persistent
  // even if the GC moves this SharedFunctionInfo.
  DECL_INT_ACCESSORS(unique_id)
#endif

  // [function data]: This field holds some additional data for function.
  // Currently it has one of:
  //  - a FunctionTemplateInfo to make benefit the API [IsApiFunction()].
  //  - a BytecodeArray for the interpreter [HasBytecodeArray()].
  //  - a InterpreterData with the BytecodeArray and a copy of the
  //    interpreter trampoline [HasInterpreterData()]
  //  - a FixedArray with Asm->Wasm conversion [HasAsmWasmData()].
  //  - a Smi containing the builtin id [HasBuiltinId()]
  //  - a PreParsedScopeData for the parser [HasPreParsedScopeData()]
  //  - a Code object otherwise [HasCodeObject()]
  DECL_ACCESSORS(function_data, Object)

  inline bool IsApiFunction() const;
  inline FunctionTemplateInfo* get_api_func_data();
  inline void set_api_func_data(FunctionTemplateInfo* data);
  inline bool HasBytecodeArray() const;
  inline BytecodeArray* GetBytecodeArray() const;
  inline void set_bytecode_array(class BytecodeArray* bytecode);
  inline Code* InterpreterTrampoline() const;
  inline bool HasInterpreterData() const;
  inline InterpreterData* interpreter_data() const;
  inline void set_interpreter_data(InterpreterData* interpreter_data);
  inline bool HasAsmWasmData() const;
  inline FixedArray* asm_wasm_data() const;
  inline void set_asm_wasm_data(FixedArray* data);
  // A brief note to clear up possible confusion:
  // builtin_id corresponds to the auto-generated
  // Builtins::Name id, while builtin_function_id corresponds to
  // BuiltinFunctionId (a manually maintained list of 'interesting' functions
  // mainly used during optimization).
  inline bool HasBuiltinId() const;
  inline int builtin_id() const;
  inline void set_builtin_id(int builtin_id);
  inline bool HasPreParsedScopeData() const;
  inline PreParsedScopeData* preparsed_scope_data() const;
  inline void set_preparsed_scope_data(PreParsedScopeData* data);
  inline void ClearPreParsedScopeData();
  inline bool HasCodeObject() const;
  inline Code* code_object() const;
  inline void set_code_object();

  // [function identifier]: This field holds an additional identifier for the
  // function.
  //  - a Smi identifying a builtin function [HasBuiltinFunctionId()].
  //  - a String identifying the function's inferred name [HasInferredName()].
  // The inferred_name is inferred from variable or property
  // assignment of this function. It is used to facilitate debugging and
  // profiling of JavaScript code written in OO style, where almost
  // all functions are anonymous but are assigned to object
  // properties.
  DECL_ACCESSORS(function_identifier, Object)

  inline bool HasBuiltinFunctionId();
  inline BuiltinFunctionId builtin_function_id();
  inline void set_builtin_function_id(BuiltinFunctionId id);
  inline bool HasInferredName();
  inline String* inferred_name();
  inline void set_inferred_name(String* inferred_name);

  // [script]: Script from which the function originates.
  DECL_ACCESSORS(script, Object)

  // The function is subject to debugging if a debug info is attached.
  inline bool HasDebugInfo() const;
  DebugInfo* GetDebugInfo() const;

  // Break infos are contained in DebugInfo, this is a convenience method
  // to simplify access.
  bool HasBreakInfo() const;
  bool BreakAtEntry() const;

  // Coverage infos are contained in DebugInfo, this is a convenience method
  // to simplify access.
  bool HasCoverageInfo() const;
  CoverageInfo* GetCoverageInfo() const;

  // [debug info]: Debug information.
  DECL_ACCESSORS(debug_info, Object)

  // Bit field containing various information collected for debugging.
  // This field is either stored on the kDebugInfo slot or inside the
  // debug info struct.
  int debugger_hints() const;
  void set_debugger_hints(int value);

  // Indicates that the function was created by the Function function.
  // Though it's anonymous, toString should treat it as if it had the name
  // "anonymous".  We don't set the name itself so that the system does not
  // see a binding for it.
  DECL_BOOLEAN_ACCESSORS(name_should_print_as_anonymous)

  // Indicates that the function is either an anonymous expression
  // or an arrow function (the name field can be set through the API,
  // which does not change this flag).
  DECL_BOOLEAN_ACCESSORS(is_anonymous_expression)

  // Indicates that the the shared function info is deserialized from cache.
  DECL_BOOLEAN_ACCESSORS(deserialized)

  // Indicates that the function cannot cause side-effects.
  DECL_BOOLEAN_ACCESSORS(has_no_side_effect)

  // Indicates that the function requires runtime side-effect checks.
  DECL_BOOLEAN_ACCESSORS(requires_runtime_side_effect_checks);

  // Indicates that |has_no_side_effect| has been computed and set.
  DECL_BOOLEAN_ACCESSORS(computed_has_no_side_effect)

  // Indicates that the function should be skipped during stepping.
  DECL_BOOLEAN_ACCESSORS(debug_is_blackboxed)

  // Indicates that |debug_is_blackboxed| has been computed and set.
  DECL_BOOLEAN_ACCESSORS(computed_debug_is_blackboxed)

  // Indicates that the function has been reported for binary code coverage.
  DECL_BOOLEAN_ACCESSORS(has_reported_binary_coverage)

  // Id assigned to the function for debugging.
  // This could also be implemented as a weak hash table.
  inline int debugging_id() const;
  inline void set_debugging_id(int value);

  // The function's name if it is non-empty, otherwise the inferred name.
  String* DebugName();

  // The function cannot cause any side effects.
  static bool HasNoSideEffect(Handle<SharedFunctionInfo> info);

  // Used for flags such as --turbo-filter.
  bool PassesFilter(const char* raw_filter);

  // Position of the 'function' token in the script source.
  DECL_INT_ACCESSORS(function_token_position)

  // [raw_start_position_and_type]: Field used to store both the source code
  // position, whether or not the function is a function expression,
  // and whether or not the function is a toplevel function. The two
  // least significants bit indicates whether the function is an
  // expression and the rest contains the source code position.
  // TODO(cbruni): start_position should be removed from SFI.
  DECL_INT_ACCESSORS(raw_start_position_and_type)

  // Position of this function in the script source.
  // TODO(cbruni): start_position should be removed from SFI.
  DECL_INT_ACCESSORS(raw_start_position)

  // End position of this function in the script source.
  // TODO(cbruni): end_position should be removed from SFI.
  DECL_INT_ACCESSORS(raw_end_position)

  // Returns true if the function has shared name.
  inline bool HasSharedName() const;

  // Is this function a named function expression in the source code.
  DECL_BOOLEAN_ACCESSORS(is_named_expression)

  // Is this function a top-level function (scripts, evals).
  DECL_BOOLEAN_ACCESSORS(is_toplevel)

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

  // Indicates if this function can be lazy compiled.
  DECL_BOOLEAN_ACCESSORS(allows_lazy_compilation)

  // Indicates the language mode.
  inline LanguageMode language_mode();
  inline void set_language_mode(LanguageMode language_mode);

  // Indicates whether the source is implicitly wrapped in a function.
  DECL_BOOLEAN_ACCESSORS(is_wrapped)

  // True if the function has any duplicated parameter names.
  DECL_BOOLEAN_ACCESSORS(has_duplicate_parameters)

  // Indicates whether the function is a native function.
  // These needs special treatment in .call and .apply since
  // null passed as the receiver should not be translated to the
  // global object.
  DECL_BOOLEAN_ACCESSORS(native)

  // Whether this function was created from a FunctionDeclaration.
  DECL_BOOLEAN_ACCESSORS(is_declaration)

  // Indicates that asm->wasm conversion failed and should not be re-attempted.
  DECL_BOOLEAN_ACCESSORS(is_asm_wasm_broken)

  inline FunctionKind kind() const;

  // Defines the index in a native context of closure's map instantiated using
  // this shared function info.
  DECL_INT_ACCESSORS(function_map_index)

  // Clear uninitialized padding space. This ensures that the snapshot content
  // is deterministic.
  inline void clear_padding();

  // Recalculates the |map_index| value after modifications of this shared info.
  inline void UpdateFunctionMapIndex();

  // Indicates whether optimizations have been disabled for this shared function
  // info. If we cannot optimize the function we disable optimization to avoid
  // spending time attempting to optimize it again.
  inline bool optimization_disabled() const;

  // The reason why optimization was disabled.
  inline BailoutReason disable_optimization_reason() const;

  // Disable (further) attempted optimization of all functions sharing this
  // shared function info.
  void DisableOptimization(BailoutReason reason);

  // This class constructor needs to call out to an instance fields
  // initializer. This flag is set when creating the
  // SharedFunctionInfo as a reminder to emit the initializer call
  // when generating code later.
  DECL_BOOLEAN_ACCESSORS(requires_instance_fields_initializer)

  // [source code]: Source code for the function.
  bool HasSourceCode() const;
  static Handle<Object> GetSourceCode(Handle<SharedFunctionInfo> shared);
  static Handle<Object> GetSourceCodeHarmony(Handle<SharedFunctionInfo> shared);

  // Tells whether this function should be subject to debugging, e.g. for
  // - scope inspection
  // - internal break points
  // - coverage and type profile
  // - error stack trace
  inline bool IsSubjectToDebugging();

  // Whether this function is defined in user-provided JavaScript code.
  inline bool IsUserJavaScript();

  // True if one can flush compiled code from this function, in such a way that
  // it can later be re-compiled.
  inline bool CanFlushCompiled() const;

  // Flush compiled data from this function, setting it back to CompileLazy and
  // clearing any feedback metadata.
  inline void FlushCompiled();

  // Check whether or not this function is inlineable.
  bool IsInlineable();

  // Source size of this function.
  int SourceSize();

  // Returns `false` if formal parameters include rest parameters, optional
  // parameters, or destructuring parameters.
  // TODO(caitp): make this a flag set during parsing
  inline bool has_simple_parameters();

  // Initialize a SharedFunctionInfo from a parsed function literal.
  static void InitFromFunctionLiteral(Handle<SharedFunctionInfo> shared_info,
                                      FunctionLiteral* lit, bool is_toplevel);

  // Sets the expected number of properties based on estimate from parser.
  void SetExpectedNofPropertiesFromEstimate(FunctionLiteral* literal);

  inline bool construct_as_builtin() const;

  // Determines and sets the ConstructAsBuiltinBit in |flags|, based on the
  // |function_data|. Must be called when creating the SFI after other fields
  // are initialized. The ConstructAsBuiltinBit determines whether
  // JSBuiltinsConstructStub or JSConstructStubGeneric should be called to
  // construct this function.
  inline void CalculateConstructAsBuiltin();

  // Dispatched behavior.
  DECL_PRINTER(SharedFunctionInfo)
  DECL_VERIFIER(SharedFunctionInfo)
#ifdef OBJECT_PRINT
  void PrintSourceCode(std::ostream& os);
#endif

  // Iterate over all shared function infos in a given script.
  class ScriptIterator {
   public:
    explicit ScriptIterator(Handle<Script> script);
    ScriptIterator(Isolate* isolate,
                   Handle<WeakFixedArray> shared_function_infos);
    SharedFunctionInfo* Next();

    // Reset the iterator to run on |script|.
    void Reset(Handle<Script> script);

   private:
    Isolate* isolate_;
    Handle<WeakFixedArray> shared_function_infos_;
    int index_;
    DISALLOW_COPY_AND_ASSIGN(ScriptIterator);
  };

  // Iterate over all shared function infos on the heap.
  class GlobalIterator {
   public:
    explicit GlobalIterator(Isolate* isolate);
    SharedFunctionInfo* Next();

   private:
    Script::Iterator script_iterator_;
    FixedArrayOfWeakCells::Iterator noscript_sfi_iterator_;
    SharedFunctionInfo::ScriptIterator sfi_iterator_;
    DisallowHeapAllocation no_gc_;
    DISALLOW_COPY_AND_ASSIGN(GlobalIterator);
  };

  DECL_CAST(SharedFunctionInfo)

  // Constants.
  static const int kDontAdaptArgumentsSentinel = -1;

#if V8_SFI_HAS_UNIQUE_ID
  static const int kUniqueIdFieldSize = kInt32Size;
#else
  // Just to not break the postmortrem support with conditional offsets
  static const int kUniqueIdFieldSize = 0;
#endif

// Layout description.
#define SHARED_FUNCTION_INFO_FIELDS(V)                     \
  /* Pointer fields. */                                    \
  V(kStartOfPointerFieldsOffset, 0)                        \
  V(kFunctionDataOffset, kPointerSize)                     \
  V(kNameOrScopeInfoOffset, kPointerSize)                  \
  V(kOuterScopeInfoOrFeedbackMetadataOffset, kPointerSize) \
  V(kScriptOffset, kPointerSize)                           \
  V(kDebugInfoOffset, kPointerSize)                        \
  V(kFunctionIdentifierOffset, kPointerSize)               \
  V(kEndOfPointerFieldsOffset, 0)                          \
  /* Raw data fields. */                                   \
  V(kFunctionLiteralIdOffset, kInt32Size)                  \
  V(kUniqueIdOffset, kUniqueIdFieldSize)                   \
  V(kLengthOffset, kInt32Size)                             \
  V(kFormalParameterCountOffset, kInt32Size)               \
  V(kExpectedNofPropertiesOffset, kInt32Size)              \
  V(kStartPositionAndTypeOffset, kInt32Size)               \
  V(kEndPositionOffset, kInt32Size)                        \
  V(kFunctionTokenPositionOffset, kInt32Size)              \
  V(kFlagsOffset, kInt32Size)                              \
  /* Total size. */                                        \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                SHARED_FUNCTION_INFO_FIELDS)
#undef SHARED_FUNCTION_INFO_FIELDS

  static const int kAlignedSize = POINTER_SIZE_ALIGN(kSize);

  typedef FixedBodyDescriptor<kStartOfPointerFieldsOffset,
                              kEndOfPointerFieldsOffset, kSize>
      BodyDescriptor;
  // No weak fields.
  typedef BodyDescriptor BodyDescriptorWeak;

// Bit fields in |raw_start_position_and_type|.
#define START_POSITION_AND_TYPE_BIT_FIELDS(V, _) \
  V(IsNamedExpressionBit, bool, 1, _)            \
  V(IsTopLevelBit, bool, 1, _)                   \
  V(StartPositionBits, int, 30, _)

  DEFINE_BIT_FIELDS(START_POSITION_AND_TYPE_BIT_FIELDS)
#undef START_POSITION_AND_TYPE_BIT_FIELDS

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _)                           \
  V(IsNativeBit, bool, 1, _)                             \
  V(IsStrictBit, bool, 1, _)                             \
  V(IsWrappedBit, bool, 1, _)                            \
  V(IsClassConstructorBit, bool, 1, _)                   \
  V(IsDerivedConstructorBit, bool, 1, _)                 \
  V(FunctionKindBits, FunctionKind, 5, _)                \
  V(HasDuplicateParametersBit, bool, 1, _)               \
  V(AllowLazyCompilationBit, bool, 1, _)                 \
  V(NeedsHomeObjectBit, bool, 1, _)                      \
  V(IsDeclarationBit, bool, 1, _)                        \
  V(IsAsmWasmBrokenBit, bool, 1, _)                      \
  V(FunctionMapIndexBits, int, 5, _)                     \
  V(DisabledOptimizationReasonBits, BailoutReason, 4, _) \
  V(RequiresInstanceFieldsInitializer, bool, 1, _)       \
  V(ConstructAsBuiltinBit, bool, 1, _)

  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  // Bailout reasons must fit in the DisabledOptimizationReason bitfield.
  STATIC_ASSERT(BailoutReason::kLastErrorMessage <=
                DisabledOptimizationReasonBits::kMax);

  STATIC_ASSERT(kLastFunctionKind <= FunctionKindBits::kMax);

// Bit positions in |debugger_hints|.
#define DEBUGGER_HINTS_BIT_FIELDS(V, _)             \
  V(IsAnonymousExpressionBit, bool, 1, _)           \
  V(NameShouldPrintAsAnonymousBit, bool, 1, _)      \
  V(IsDeserializedBit, bool, 1, _)                  \
  V(HasNoSideEffectBit, bool, 1, _)                 \
  V(RequiresRuntimeSideEffectChecksBit, bool, 1, _) \
  V(ComputedHasNoSideEffectBit, bool, 1, _)         \
  V(DebugIsBlackboxedBit, bool, 1, _)               \
  V(ComputedDebugIsBlackboxedBit, bool, 1, _)       \
  V(HasReportedBinaryCoverageBit, bool, 1, _)       \
  V(DebuggingIdBits, int, 20, _)

  DEFINE_BIT_FIELDS(DEBUGGER_HINTS_BIT_FIELDS)
#undef DEBUGGER_HINTS_BIT_FIELDS

  static const int kNoDebuggingId = 0;

  // Indicates that this function uses a super property (or an eval that may
  // use a super property).
  // This is needed to set up the [[HomeObject]] on the function instance.
  inline bool needs_home_object() const;

 private:
  // [name_or_scope_info]: Function name string, kNoSharedNameSentinel or
  // ScopeInfo.
  DECL_ACCESSORS(name_or_scope_info, Object)

  // [outer scope info] The outer scope info, needed to lazily parse this
  // function.
  DECL_ACCESSORS(outer_scope_info, HeapObject)

  inline void set_kind(FunctionKind kind);

  inline void set_needs_home_object(bool value);

  friend class Factory;
  friend class V8HeapExplorer;
  FRIEND_TEST(PreParserTest, LazyFunctionLength);

  inline int length() const;

  DISALLOW_IMPLICIT_CONSTRUCTORS(SharedFunctionInfo);
};

// Printing support.
struct SourceCodeOf {
  explicit SourceCodeOf(SharedFunctionInfo* v, int max = -1)
      : value(v), max_length(max) {}
  const SharedFunctionInfo* value;
  int max_length;
};

std::ostream& operator<<(std::ostream& os, const SourceCodeOf& v);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SHARED_FUNCTION_INFO_H_
