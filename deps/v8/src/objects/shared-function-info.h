// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SHARED_FUNCTION_INFO_H_
#define V8_OBJECTS_SHARED_FUNCTION_INFO_H_

#include "src/bailout-reason.h"
#include "src/objects.h"
#include "src/objects/builtin-function-id.h"
#include "src/objects/script.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class BytecodeArray;
class CoverageInfo;
class DebugInfo;
class WasmExportedFunctionData;

// Data collected by the pre-parser storing information about scopes and inner
// functions.
class PreParsedScopeData : public HeapObject {
 public:
  DECL_ACCESSORS(scope_data, PodArray<uint8_t>)
  DECL_INT_ACCESSORS(length)

  inline Object* child_data(int index) const;
  inline void set_child_data(int index, Object* value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Object** child_data_start() const;

  // Clear uninitialized padding space.
  inline void clear_padding();

  DECL_CAST(PreParsedScopeData)
  DECL_PRINTER(PreParsedScopeData)
  DECL_VERIFIER(PreParsedScopeData)

#define PRE_PARSED_SCOPE_DATA_FIELDS(V) \
  V(kScopeDataOffset, kPointerSize)     \
  V(kLengthOffset, kIntSize)            \
  V(kUnalignedChildDataStartOffset, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                PRE_PARSED_SCOPE_DATA_FIELDS)
#undef PRE_PARSED_SCOPE_DATA_FIELDS

  static const int kChildDataStartOffset =
      POINTER_SIZE_ALIGN(kUnalignedChildDataStartOffset);

  class BodyDescriptor;

  static constexpr int SizeFor(int length) {
    return kChildDataStartOffset + length * kPointerSize;
  }

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(PreParsedScopeData);
};

// Abstract class representing extra data for an uncompiled function, which is
// not stored in the SharedFunctionInfo.
class UncompiledData : public HeapObject {
 public:
  DECL_ACCESSORS(inferred_name, String)
  DECL_INT32_ACCESSORS(start_position)
  DECL_INT32_ACCESSORS(end_position)
  DECL_INT32_ACCESSORS(function_literal_id)

  DECL_CAST(UncompiledData)

#define UNCOMPILED_DATA_FIELDS(V)         \
  V(kStartOfPointerFieldsOffset, 0)       \
  V(kInferredNameOffset, kPointerSize)    \
  V(kEndOfPointerFieldsOffset, 0)         \
  V(kStartPositionOffset, kInt32Size)     \
  V(kEndPositionOffset, kInt32Size)       \
  V(kFunctionLiteralIdOffset, kInt32Size) \
  /* Total size. */                       \
  V(kUnalignedSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize, UNCOMPILED_DATA_FIELDS)
#undef UNCOMPILED_DATA_FIELDS

  static const int kSize = POINTER_SIZE_ALIGN(kUnalignedSize);

  typedef FixedBodyDescriptor<kStartOfPointerFieldsOffset,
                              kEndOfPointerFieldsOffset, kSize>
      BodyDescriptor;

  // Clear uninitialized padding space.
  inline void clear_padding();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UncompiledData);
};

// Class representing data for an uncompiled function that does not have any
// data from the pre-parser, either because it's a leaf function or because the
// pre-parser bailed out.
class UncompiledDataWithoutPreParsedScope : public UncompiledData {
 public:
  DECL_CAST(UncompiledDataWithoutPreParsedScope)
  DECL_PRINTER(UncompiledDataWithoutPreParsedScope)
  DECL_VERIFIER(UncompiledDataWithoutPreParsedScope)

  static const int kSize = UncompiledData::kSize;

  // No extra fields compared to UncompiledData.
  typedef UncompiledData::BodyDescriptor BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UncompiledDataWithoutPreParsedScope);
};

// Class representing data for an uncompiled function that has pre-parsed scope
// data.
class UncompiledDataWithPreParsedScope : public UncompiledData {
 public:
  DECL_ACCESSORS(pre_parsed_scope_data, PreParsedScopeData)

  DECL_CAST(UncompiledDataWithPreParsedScope)
  DECL_PRINTER(UncompiledDataWithPreParsedScope)
  DECL_VERIFIER(UncompiledDataWithPreParsedScope)

#define UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_FIELDS(V) \
  V(kStartOfPointerFieldsOffset, 0)                     \
  V(kPreParsedScopeDataOffset, kPointerSize)            \
  V(kEndOfPointerFieldsOffset, 0)                       \
  /* Total size. */                                     \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(UncompiledData::kSize,
                                UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_FIELDS)
#undef UNCOMPILED_DATA_WITH_PRE_PARSED_SCOPE_FIELDS

  // Make sure the size is aligned
  STATIC_ASSERT(kSize == POINTER_SIZE_ALIGN(kSize));

  typedef SubclassBodyDescriptor<
      UncompiledData::BodyDescriptor,
      FixedBodyDescriptor<kStartOfPointerFieldsOffset,
                          kEndOfPointerFieldsOffset, kSize>>
      BodyDescriptor;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(UncompiledDataWithPreParsedScope);
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
class SharedFunctionInfo : public HeapObject, public NeverReadOnlySpaceObject {
 public:
  static constexpr Object* const kNoSharedNameSentinel = Smi::kZero;

  // [name]: Returns shared name if it exists or an empty string otherwise.
  inline String* Name() const;
  inline void SetName(String* name);

  // Get the code object which represents the execution of this function.
  Code* GetCode() const;

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
      int function_literal_id, bool reset_preparsed_scope_data = true);

  // Layout description of the optimized code map.
  static const int kEntriesStart = 0;
  static const int kContextOffset = 0;
  static const int kCachedCodeOffset = 1;
  static const int kEntryLength = 2;
  static const int kInitialLength = kEntriesStart + kEntryLength;

  static const int kNotFound = -1;
  static const uint16_t kInvalidLength = static_cast<uint16_t>(-1);

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
  V8_EXPORT_PRIVATE int EndPosition() const;

  // Start position of this function in the script source.
  V8_EXPORT_PRIVATE int StartPosition() const;

  // Set the start and end position of this function in the script source.
  // Updates the scope info if available.
  V8_EXPORT_PRIVATE void SetPosition(int start_position, int end_position);

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
  // Use up to 2^16-2 parameters (16 bits of values, where one is reserved for
  // kDontAdaptArgumentsSentinel). The value is only reliable when the function
  // has been compiled.
  inline uint16_t GetLength() const;
  inline bool HasLength() const;
  inline void set_length(int value);

  // [internal formal parameter count]: The declared number of parameters.
  // For subclass constructors, also includes new.target.
  // The size of function's frame is internal_formal_parameter_count + 1.
  DECL_UINT16_ACCESSORS(internal_formal_parameter_count)

  // Set the formal parameter count so the function code will be
  // called without using argument adaptor frames.
  inline void DontAdaptArguments();

  // [expected_nof_properties]: Expected number of properties for the
  // function. The value is only reliable when the function has been compiled.
  DECL_UINT8_ACCESSORS(expected_nof_properties)

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
  //  - a UncompiledDataWithoutPreParsedScope for lazy compilation
  //    [HasUncompiledDataWithoutPreParsedScope()]
  //  - a UncompiledDataWithPreParsedScope for lazy compilation
  //    [HasUncompiledDataWithPreParsedScope()]
  //  - a WasmExportedFunctionData for Wasm [HasWasmExportedFunctionData()]
  DECL_ACCESSORS(function_data, Object)

  inline bool IsApiFunction() const;
  inline FunctionTemplateInfo* get_api_func_data();
  inline void set_api_func_data(FunctionTemplateInfo* data);
  inline bool HasBytecodeArray() const;
  inline BytecodeArray* GetBytecodeArray() const;
  inline void set_bytecode_array(BytecodeArray* bytecode);
  inline Code* InterpreterTrampoline() const;
  inline bool HasInterpreterData() const;
  inline InterpreterData* interpreter_data() const;
  inline void set_interpreter_data(InterpreterData* interpreter_data);
  inline BytecodeArray* GetDebugBytecodeArray() const;
  inline void SetDebugBytecodeArray(BytecodeArray* bytecode);
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
  inline bool HasUncompiledData() const;
  inline UncompiledData* uncompiled_data() const;
  inline void set_uncompiled_data(UncompiledData* data);
  inline bool HasUncompiledDataWithPreParsedScope() const;
  inline UncompiledDataWithPreParsedScope*
  uncompiled_data_with_pre_parsed_scope() const;
  inline void set_uncompiled_data_with_pre_parsed_scope(
      UncompiledDataWithPreParsedScope* data);
  inline bool HasUncompiledDataWithoutPreParsedScope() const;
  inline bool HasWasmExportedFunctionData() const;
  WasmExportedFunctionData* wasm_exported_function_data() const;
  inline void set_wasm_exported_function_data(WasmExportedFunctionData* data);

  // Clear out pre-parsed scope data from UncompiledDataWithPreParsedScope,
  // turning it into UncompiledDataWithoutPreParsedScope.
  inline void ClearPreParsedScopeData();

  // [raw_builtin_function_id]: The id of the built-in function this function
  // represents, used during optimization to improve code generation.
  // TODO(leszeks): Once there are no more JS builtins, this can be replaced
  // by BuiltinId.
  DECL_UINT8_ACCESSORS(raw_builtin_function_id)
  inline bool HasBuiltinFunctionId();
  inline BuiltinFunctionId builtin_function_id();
  inline void set_builtin_function_id(BuiltinFunctionId id);
  // Make sure BuiltinFunctionIds fit in a uint8_t
  STATIC_ASSERT((std::is_same<std::underlying_type<BuiltinFunctionId>::type,
                              uint8_t>::value));

  // The inferred_name is inferred from variable or property assignment of this
  // function. It is used to facilitate debugging and profiling of JavaScript
  // code written in OO style, where almost all functions are anonymous but are
  // assigned to object properties.
  inline bool HasInferredName();
  inline String* inferred_name();

  // Get the function literal id associated with this function, for parsing.
  V8_EXPORT_PRIVATE int FunctionLiteralId(Isolate* isolate) const;

  // Break infos are contained in DebugInfo, this is a convenience method
  // to simplify access.
  bool HasBreakInfo() const;
  bool BreakAtEntry() const;

  // Coverage infos are contained in DebugInfo, this is a convenience method
  // to simplify access.
  bool HasCoverageInfo() const;
  CoverageInfo* GetCoverageInfo() const;

  // The function's name if it is non-empty, otherwise the inferred name.
  String* DebugName();

  // Used for flags such as --turbo-filter.
  bool PassesFilter(const char* raw_filter);

  // [script_or_debug_info]: One of:
  //  - Script from which the function originates.
  //  - a DebugInfo which holds the actual script [HasDebugInfo()].
  DECL_ACCESSORS(script_or_debug_info, Object)

  inline Object* script() const;
  inline void set_script(Object* script);

  // The function is subject to debugging if a debug info is attached.
  inline bool HasDebugInfo() const;
  inline DebugInfo* GetDebugInfo() const;
  inline void SetDebugInfo(DebugInfo* debug_info);

  // The offset of the 'function' token in the script source relative to the
  // start position. Can return kFunctionTokenOutOfRange if offset doesn't
  // fit in 16 bits.
  DECL_UINT16_ACCESSORS(raw_function_token_offset)

  // The position of the 'function' token in the script source. Can return
  // kNoSourcePosition if raw_function_token_offset() returns
  // kFunctionTokenOutOfRange.
  inline int function_token_position() const;

  // Returns true if the function has shared name.
  inline bool HasSharedName() const;

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

  // Is this function a named function expression in the source code.
  DECL_BOOLEAN_ACCESSORS(is_named_expression)

  // Is this function a top-level function (scripts, evals).
  DECL_BOOLEAN_ACCESSORS(is_toplevel)

  // Indicates if this function can be lazy compiled.
  DECL_BOOLEAN_ACCESSORS(allows_lazy_compilation)

  // Indicates the language mode.
  inline LanguageMode language_mode() const;
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

  // Indicates that the function has been reported for binary code coverage.
  DECL_BOOLEAN_ACCESSORS(has_reported_binary_coverage)

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
  inline bool CanDiscardCompiled() const;

  // Flush compiled data from this function, setting it back to CompileLazy and
  // clearing any feedback metadata.
  static inline void DiscardCompiled(Isolate* isolate,
                                     Handle<SharedFunctionInfo> shared_info);

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

  // Sets the FunctionTokenOffset field based on the given token position and
  // start position.
  void SetFunctionTokenPosition(int function_token_position,
                                int start_position);

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
    ScriptIterator(Isolate* isolate, Script* script);
    ScriptIterator(Isolate* isolate,
                   Handle<WeakFixedArray> shared_function_infos);
    SharedFunctionInfo* Next();
    int CurrentIndex() const { return index_ - 1; }

    // Reset the iterator to run on |script|.
    void Reset(Script* script);

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
    WeakArrayList::Iterator noscript_sfi_iterator_;
    SharedFunctionInfo::ScriptIterator sfi_iterator_;
    DisallowHeapAllocation no_gc_;
    DISALLOW_COPY_AND_ASSIGN(GlobalIterator);
  };

  DECL_CAST(SharedFunctionInfo)

  // Constants.
  static const uint16_t kDontAdaptArgumentsSentinel = static_cast<uint16_t>(-1);

  static const int kMaximumFunctionTokenOffset = kMaxUInt16 - 1;
  static const uint16_t kFunctionTokenOutOfRange = static_cast<uint16_t>(-1);
  STATIC_ASSERT(kMaximumFunctionTokenOffset + 1 == kFunctionTokenOutOfRange);

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
  V(kScriptOrDebugInfoOffset, kPointerSize)                \
  V(kEndOfPointerFieldsOffset, 0)                          \
  /* Raw data fields. */                                   \
  V(kUniqueIdOffset, kUniqueIdFieldSize)                   \
  V(kLengthOffset, kUInt16Size)                            \
  V(kFormalParameterCountOffset, kUInt16Size)              \
  V(kExpectedNofPropertiesOffset, kUInt8Size)              \
  V(kBuiltinFunctionId, kUInt8Size)                        \
  V(kFunctionTokenOffsetOffset, kUInt16Size)               \
  V(kFlagsOffset, kInt32Size)                              \
  /* Total size. */                                        \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(HeapObject::kHeaderSize,
                                SHARED_FUNCTION_INFO_FIELDS)
#undef SHARED_FUNCTION_INFO_FIELDS

  static const int kAlignedSize = POINTER_SIZE_ALIGN(kSize);

  typedef FixedBodyDescriptor<kStartOfPointerFieldsOffset,
                              kEndOfPointerFieldsOffset, kAlignedSize>
      BodyDescriptor;

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
  V(ConstructAsBuiltinBit, bool, 1, _)                   \
  V(IsAnonymousExpressionBit, bool, 1, _)                \
  V(NameShouldPrintAsAnonymousBit, bool, 1, _)           \
  V(IsDeserializedBit, bool, 1, _)                       \
  V(HasReportedBinaryCoverageBit, bool, 1, _)            \
  V(IsNamedExpressionBit, bool, 1, _)                    \
  V(IsTopLevelBit, bool, 1, _)
  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  // Bailout reasons must fit in the DisabledOptimizationReason bitfield.
  STATIC_ASSERT(BailoutReason::kLastErrorMessage <=
                DisabledOptimizationReasonBits::kMax);

  STATIC_ASSERT(kLastFunctionKind <= FunctionKindBits::kMax);

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

  inline uint16_t length() const;

  // Find the index of this function in the parent script. Slow path of
  // FunctionLiteralId.
  int FindIndexInScript(Isolate* isolate) const;

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
