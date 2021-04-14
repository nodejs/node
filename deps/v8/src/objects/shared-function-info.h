// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SHARED_FUNCTION_INFO_H_
#define V8_OBJECTS_SHARED_FUNCTION_INFO_H_

#include <memory>

#include "src/base/bit-field.h"
#include "src/codegen/bailout-reason.h"
#include "src/objects/compressed-slots.h"
#include "src/objects/function-kind.h"
#include "src/objects/function-syntax-kind.h"
#include "src/objects/objects.h"
#include "src/objects/script.h"
#include "src/objects/slots.h"
#include "src/objects/smi.h"
#include "src/objects/struct.h"
#include "src/roots/roots.h"
#include "testing/gtest/include/gtest/gtest_prod.h"  // nogncheck
#include "torque-generated/bit-fields.h"
#include "torque-generated/field-offsets.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {

namespace internal {

class AsmWasmData;
class BytecodeArray;
class CoverageInfo;
class DebugInfo;
class IsCompiledScope;
template <typename>
class Signature;
class WasmCapiFunctionData;
class WasmExportedFunctionData;
class WasmJSFunctionData;

namespace wasm {
struct WasmModule;
class ValueType;
using FunctionSig = Signature<ValueType>;
}  // namespace wasm

#include "torque-generated/src/objects/shared-function-info-tq.inc"

// Data collected by the pre-parser storing information about scopes and inner
// functions.
//
// PreparseData Layout:
// +-------------------------------+
// | data_length | children_length |
// +-------------------------------+
// | Scope Byte Data ...           |
// | ...                           |
// +-------------------------------+
// | [Padding]                     |
// +-------------------------------+
// | Inner PreparseData 1          |
// +-------------------------------+
// | ...                           |
// +-------------------------------+
// | Inner PreparseData N          |
// +-------------------------------+
class PreparseData
    : public TorqueGeneratedPreparseData<PreparseData, HeapObject> {
 public:
  inline int inner_start_offset() const;
  inline ObjectSlot inner_data_start() const;

  inline byte get(int index) const;
  inline void set(int index, byte value);
  inline void copy_in(int index, const byte* buffer, int length);

  inline PreparseData get_child(int index) const;
  inline void set_child(int index, PreparseData value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Clear uninitialized padding space.
  inline void clear_padding();

  DECL_PRINTER(PreparseData)
  DECL_VERIFIER(PreparseData)

  static const int kDataStartOffset = kSize;

  class BodyDescriptor;

  static int InnerOffset(int data_length) {
    return RoundUp(kDataStartOffset + data_length * kByteSize, kTaggedSize);
  }

  static int SizeFor(int data_length, int children_length) {
    return InnerOffset(data_length) + children_length * kTaggedSize;
  }

  TQ_OBJECT_CONSTRUCTORS(PreparseData)

 private:
  inline Object get_child_raw(int index) const;
};

// Abstract class representing extra data for an uncompiled function, which is
// not stored in the SharedFunctionInfo.
class UncompiledData
    : public TorqueGeneratedUncompiledData<UncompiledData, HeapObject> {
 public:
  inline void InitAfterBytecodeFlush(
      String inferred_name, int start_position, int end_position,
      std::function<void(HeapObject object, ObjectSlot slot, HeapObject target)>
          gc_notify_updated_slot);

  TQ_OBJECT_CONSTRUCTORS(UncompiledData)
};

// Class representing data for an uncompiled function that does not have any
// data from the pre-parser, either because it's a leaf function or because the
// pre-parser bailed out.
class UncompiledDataWithoutPreparseData
    : public TorqueGeneratedUncompiledDataWithoutPreparseData<
          UncompiledDataWithoutPreparseData, UncompiledData> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(UncompiledDataWithoutPreparseData)
};

// Class representing data for an uncompiled function that has pre-parsed scope
// data.
class UncompiledDataWithPreparseData
    : public TorqueGeneratedUncompiledDataWithPreparseData<
          UncompiledDataWithPreparseData, UncompiledData> {
 public:
  class BodyDescriptor;

  TQ_OBJECT_CONSTRUCTORS(UncompiledDataWithPreparseData)
};

class InterpreterData : public Struct {
 public:
  DECL_ACCESSORS(bytecode_array, BytecodeArray)
  DECL_ACCESSORS(interpreter_trampoline, Code)

  DEFINE_FIELD_OFFSET_CONSTANTS(Struct::kHeaderSize,
                                TORQUE_GENERATED_INTERPRETER_DATA_FIELDS)

  DECL_CAST(InterpreterData)
  DECL_PRINTER(InterpreterData)
  DECL_VERIFIER(InterpreterData)

  OBJECT_CONSTRUCTORS(InterpreterData, Struct);
};

class BaselineData : public TorqueGeneratedBaselineData<BaselineData, Struct> {
 public:
  inline BytecodeArray GetActiveBytecodeArray() const;
  inline void SetActiveBytecodeArray(BytecodeArray bytecode);

  TQ_OBJECT_CONSTRUCTORS(BaselineData)
};

// SharedFunctionInfo describes the JSFunction information that can be
// shared by multiple instances of the function.
class SharedFunctionInfo
    : public TorqueGeneratedSharedFunctionInfo<SharedFunctionInfo, HeapObject> {
 public:
  NEVER_READ_ONLY_SPACE
  DEFINE_TORQUE_GENERATED_SHARED_FUNCTION_INFO_FLAGS()
  DEFINE_TORQUE_GENERATED_SHARED_FUNCTION_INFO_FLAGS2()

  // This initializes the SharedFunctionInfo after allocation. It must
  // initialize all fields, and leave the SharedFunctionInfo in a state where
  // it is safe for the GC to visit it.
  //
  // Important: This function MUST not allocate.
  void Init(ReadOnlyRoots roots, int unique_id);

  V8_EXPORT_PRIVATE static constexpr Smi const kNoSharedNameSentinel =
      Smi::zero();

  // [name]: Returns shared name if it exists or an empty string otherwise.
  inline String Name() const;
  inline void SetName(String name);

  // Get the code object which represents the execution of this function.
  V8_EXPORT_PRIVATE Code GetCode() const;

  // Get the abstract code associated with the function, which will either be
  // a Code object or a BytecodeArray.
  template <typename LocalIsolate>
  inline AbstractCode abstract_code(LocalIsolate* isolate);

  // Tells whether or not this shared function info has an attached
  // BytecodeArray.
  inline bool IsInterpreted() const;

  // Set up the link between shared function info and the script. The shared
  // function info is added to the list on the script.
  V8_EXPORT_PRIVATE void SetScript(ReadOnlyRoots roots,
                                   HeapObject script_object,
                                   int function_literal_id,
                                   bool reset_preparsed_scope_data = true);

  // Layout description of the optimized code map.
  static const int kEntriesStart = 0;
  static const int kContextOffset = 0;
  static const int kCachedCodeOffset = 1;
  static const int kEntryLength = 2;
  static const int kInitialLength = kEntriesStart + kEntryLength;

  static const int kNotFound = -1;

  DECL_GETTER(scope_info, ScopeInfo)

  // Set scope_info without moving the existing name onto the ScopeInfo.
  inline void set_raw_scope_info(ScopeInfo scope_info,
                                 WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline void SetScopeInfo(ScopeInfo scope_info,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline bool is_script() const;
  inline bool needs_script_context() const;

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
 private:
  using TorqueGeneratedSharedFunctionInfo::
      outer_scope_info_or_feedback_metadata;
  using TorqueGeneratedSharedFunctionInfo::
      set_outer_scope_info_or_feedback_metadata;

 public:
  // Get the outer scope info whether this function is compiled or not.
  inline bool HasOuterScopeInfo() const;
  inline ScopeInfo GetOuterScopeInfo() const;

  // [feedback metadata] Metadata template for feedback vectors of instances of
  // this function.
  inline bool HasFeedbackMetadata() const;
  DECL_ACCESSORS(feedback_metadata, FeedbackMetadata)

  // Returns if this function has been compiled yet. Note: with bytecode
  // flushing, any GC after this call is made could cause the function
  // to become uncompiled. If you need to ensure the function remains compiled
  // for some period of time, use IsCompiledScope instead.
  inline bool is_compiled() const;

  // Returns an IsCompiledScope which reports whether the function is compiled,
  // and if compiled, will avoid the function becoming uncompiled while it is
  // held.
  template <typename LocalIsolate>
  inline IsCompiledScope is_compiled_scope(LocalIsolate* isolate) const;


  // [internal formal parameter count]: The declared number of parameters.
  // For subclass constructors, also includes new.target.
  // The size of function's frame is internal_formal_parameter_count + 1.
  DECL_UINT16_ACCESSORS(internal_formal_parameter_count)
 private:
  using TorqueGeneratedSharedFunctionInfo::formal_parameter_count;
  using TorqueGeneratedSharedFunctionInfo::set_formal_parameter_count;

 public:
  // Set the formal parameter count so the function code will be
  // called without using argument adaptor frames.
  inline void DontAdaptArguments();

  // [function data]: This field holds some additional data for function.
  // Currently it has one of:
  //  - a FunctionTemplateInfo to make benefit the API [IsApiFunction()].
  //  - a BytecodeArray for the interpreter [HasBytecodeArray()].
  //  - a InterpreterData with the BytecodeArray and a copy of the
  //    interpreter trampoline [HasInterpreterData()]
  //  - an AsmWasmData with Asm->Wasm conversion [HasAsmWasmData()].
  //  - a Smi containing the builtin id [HasBuiltinId()]
  //  - a UncompiledDataWithoutPreparseData for lazy compilation
  //    [HasUncompiledDataWithoutPreparseData()]
  //  - a UncompiledDataWithPreparseData for lazy compilation
  //    [HasUncompiledDataWithPreparseData()]
  //  - a WasmExportedFunctionData for Wasm [HasWasmExportedFunctionData()]
  DECL_RELEASE_ACQUIRE_ACCESSORS(function_data, Object)

  inline bool IsApiFunction() const;
  inline bool is_class_constructor() const;
  inline FunctionTemplateInfo get_api_func_data() const;
  inline void set_api_func_data(FunctionTemplateInfo data);
  inline bool HasBytecodeArray() const;
  template <typename LocalIsolate>
  inline BytecodeArray GetBytecodeArray(LocalIsolate* isolate) const;

  inline void set_bytecode_array(BytecodeArray bytecode);
  inline Code InterpreterTrampoline() const;
  inline bool HasInterpreterData() const;
  inline InterpreterData interpreter_data() const;
  inline void set_interpreter_data(InterpreterData interpreter_data);
  inline bool HasBaselineData() const;
  inline BaselineData baseline_data() const;
  inline void set_baseline_data(BaselineData Baseline_data);
  inline void flush_baseline_data();
  inline BytecodeArray GetActiveBytecodeArray() const;
  inline void SetActiveBytecodeArray(BytecodeArray bytecode);

#if V8_ENABLE_WEBASSEMBLY
  inline bool HasAsmWasmData() const;
  inline bool HasWasmExportedFunctionData() const;
  inline bool HasWasmJSFunctionData() const;
  inline bool HasWasmCapiFunctionData() const;
  inline AsmWasmData asm_wasm_data() const;
  inline void set_asm_wasm_data(AsmWasmData data);

  V8_EXPORT_PRIVATE WasmExportedFunctionData
  wasm_exported_function_data() const;
  WasmJSFunctionData wasm_js_function_data() const;
  WasmCapiFunctionData wasm_capi_function_data() const;

  inline const wasm::WasmModule* wasm_module() const;
  inline const wasm::FunctionSig* wasm_function_signature() const;
#endif  // V8_ENABLE_WEBASSEMBLY

  // builtin_id corresponds to the auto-generated Builtins::Name id.
  inline bool HasBuiltinId() const;
  inline int builtin_id() const;
  inline void set_builtin_id(int builtin_id);
  inline bool HasUncompiledData() const;
  inline UncompiledData uncompiled_data() const;
  inline void set_uncompiled_data(UncompiledData data);
  inline bool HasUncompiledDataWithPreparseData() const;
  inline UncompiledDataWithPreparseData uncompiled_data_with_preparse_data()
      const;
  inline void set_uncompiled_data_with_preparse_data(
      UncompiledDataWithPreparseData data);
  inline bool HasUncompiledDataWithoutPreparseData() const;

  // Clear out pre-parsed scope data from UncompiledDataWithPreparseData,
  // turning it into UncompiledDataWithoutPreparseData.
  inline void ClearPreparseData();

  // The inferred_name is inferred from variable or property assignment of this
  // function. It is used to facilitate debugging and profiling of JavaScript
  // code written in OO style, where almost all functions are anonymous but are
  // assigned to object properties.
  inline bool HasInferredName();
  inline String inferred_name();

  // Break infos are contained in DebugInfo, this is a convenience method
  // to simplify access.
  V8_EXPORT_PRIVATE bool HasBreakInfo() const;
  bool BreakAtEntry() const;

  // Coverage infos are contained in DebugInfo, this is a convenience method
  // to simplify access.
  bool HasCoverageInfo() const;
  CoverageInfo GetCoverageInfo() const;

  // The function's name if it is non-empty, otherwise the inferred name.
  std::unique_ptr<char[]> DebugNameCStr();
  static Handle<String> DebugName(Handle<SharedFunctionInfo>);

  // Used for flags such as --turbo-filter.
  bool PassesFilter(const char* raw_filter);

  // [script_or_debug_info]: One of:
  //  - Script from which the function originates.
  //  - a DebugInfo which holds the actual script [HasDebugInfo()].
  DECL_RELEASE_ACQUIRE_ACCESSORS(script_or_debug_info, HeapObject)

  inline HeapObject script() const;
  inline void set_script(HeapObject script);

  // True if the underlying script was parsed and compiled in REPL mode.
  inline bool is_repl_mode() const;

  // The function is subject to debugging if a debug info is attached.
  inline bool HasDebugInfo() const;
  inline DebugInfo GetDebugInfo() const;
  inline void SetDebugInfo(DebugInfo debug_info);

  // The offset of the 'function' token in the script source relative to the
  // start position. Can return kFunctionTokenOutOfRange if offset doesn't
  // fit in 16 bits.
  DECL_UINT16_ACCESSORS(raw_function_token_offset)
 private:
  using TorqueGeneratedSharedFunctionInfo::function_token_offset;
  using TorqueGeneratedSharedFunctionInfo::set_function_token_offset;

 public:
  // The position of the 'function' token in the script source. Can return
  // kNoSourcePosition if raw_function_token_offset() returns
  // kFunctionTokenOutOfRange.
  inline int function_token_position() const;

  // Returns true if the function has shared name.
  inline bool HasSharedName() const;

  // [flags] Bit field containing various flags about the function.
  DECL_INT32_ACCESSORS(flags)
  DECL_UINT8_ACCESSORS(flags2)

  // True if the outer class scope contains a private brand for
  // private instance methdos.
  DECL_BOOLEAN_ACCESSORS(class_scope_has_private_brand)
  DECL_BOOLEAN_ACCESSORS(has_static_private_methods_or_accessors)

  // True if a Code object associated with this SFI has been inserted into the
  // compilation cache. Note that the cache entry may be removed by aging,
  // hence the 'may'.
  DECL_BOOLEAN_ACCESSORS(may_have_cached_code)

  // Returns the cached Code object for this SFI if it exists, an empty handle
  // otherwise.
  MaybeHandle<Code> TryGetCachedCode(Isolate* isolate);

  // Is this function a top-level function (scripts, evals).
  DECL_BOOLEAN_ACCESSORS(is_toplevel)

  // Indicates if this function can be lazy compiled.
  DECL_BOOLEAN_ACCESSORS(allows_lazy_compilation)

  // Indicates the language mode.
  inline LanguageMode language_mode() const;
  inline void set_language_mode(LanguageMode language_mode);

  // How the function appears in source text.
  DECL_PRIMITIVE_ACCESSORS(syntax_kind, FunctionSyntaxKind)

  // Indicates whether the source is implicitly wrapped in a function.
  inline bool is_wrapped() const;

  // True if the function has any duplicated parameter names.
  DECL_BOOLEAN_ACCESSORS(has_duplicate_parameters)

  // Indicates whether the function is a native function.
  // These needs special treatment in .call and .apply since
  // null passed as the receiver should not be translated to the
  // global object.
  DECL_BOOLEAN_ACCESSORS(native)

#if V8_ENABLE_WEBASSEMBLY
  // Indicates that asm->wasm conversion failed and should not be re-attempted.
  DECL_BOOLEAN_ACCESSORS(is_asm_wasm_broken)
#endif  // V8_ENABLE_WEBASSEMBLY

  // Indicates that the function was created by the Function function.
  // Though it's anonymous, toString should treat it as if it had the name
  // "anonymous".  We don't set the name itself so that the system does not
  // see a binding for it.
  DECL_BOOLEAN_ACCESSORS(name_should_print_as_anonymous)

  // Indicates that the function represented by the shared function info was
  // classed as an immediately invoked function execution (IIFE) function and
  // is only executed once.
  DECL_BOOLEAN_ACCESSORS(is_oneshot_iife)

  // Whether or not the number of expected properties may change.
  DECL_BOOLEAN_ACCESSORS(are_properties_final)

  // Indicates that the function has been reported for binary code coverage.
  DECL_BOOLEAN_ACCESSORS(has_reported_binary_coverage)

  // Indicates that the private name lookups inside the function skips the
  // closest outer class scope.
  DECL_BOOLEAN_ACCESSORS(private_name_lookup_skips_outer_class)

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
  DECL_BOOLEAN_ACCESSORS(requires_instance_members_initializer)

  // [source code]: Source code for the function.
  bool HasSourceCode() const;
  static Handle<Object> GetSourceCode(Handle<SharedFunctionInfo> shared);
  static Handle<Object> GetSourceCodeHarmony(Handle<SharedFunctionInfo> shared);

  // Tells whether this function should be subject to debugging, e.g. for
  // - scope inspection
  // - internal break points
  // - coverage and type profile
  // - error stack trace
  inline bool IsSubjectToDebugging() const;

  // Whether this function is defined in user-provided JavaScript code.
  inline bool IsUserJavaScript() const;

  // True if one can flush compiled code from this function, in such a way that
  // it can later be re-compiled.
  inline bool CanDiscardCompiled() const;

  // Flush compiled data from this function, setting it back to CompileLazy and
  // clearing any compiled metadata.
  V8_EXPORT_PRIVATE static void DiscardCompiled(
      Isolate* isolate, Handle<SharedFunctionInfo> shared_info);

  // Discard the compiled metadata. If called during GC then
  // |gc_notify_updated_slot| should be used to record any slot updates.
  void DiscardCompiledMetadata(
      Isolate* isolate,
      std::function<void(HeapObject object, ObjectSlot slot, HeapObject target)>
          gc_notify_updated_slot =
              [](HeapObject object, ObjectSlot slot, HeapObject target) {});

  // Returns true if the function has old bytecode that could be flushed. This
  // function shouldn't access any flags as it is used by concurrent marker.
  // Hence it takes the mode as an argument.
  inline bool ShouldFlushBytecode(BytecodeFlushMode mode);

  enum Inlineability {
    kIsInlineable,
    // Different reasons for not being inlineable:
    kHasNoScript,
    kNeedsBinaryCoverage,
    kHasOptimizationDisabled,
    kIsBuiltin,
    kIsNotUserCode,
    kHasNoBytecode,
    kExceedsBytecodeLimit,
    kMayContainBreakPoints,
  };
  template <typename LocalIsolate>
  Inlineability GetInlineability(LocalIsolate* isolate) const;

  // Source size of this function.
  int SourceSize();

  // Returns `false` if formal parameters include rest parameters, optional
  // parameters, or destructuring parameters.
  // TODO(caitp): make this a flag set during parsing
  inline bool has_simple_parameters();

  // Initialize a SharedFunctionInfo from a parsed function literal.
  template <typename LocalIsolate>
  static void InitFromFunctionLiteral(LocalIsolate* isolate,
                                      Handle<SharedFunctionInfo> shared_info,
                                      FunctionLiteral* lit, bool is_toplevel);

  // Updates the expected number of properties based on estimate from parser.
  void UpdateExpectedNofPropertiesFromEstimate(FunctionLiteral* literal);
  void UpdateAndFinalizeExpectedNofPropertiesFromEstimate(
      FunctionLiteral* literal);

  // Sets the FunctionTokenOffset field based on the given token position and
  // start position.
  void SetFunctionTokenPosition(int function_token_position,
                                int start_position);

  static void EnsureSourcePositionsAvailable(
      Isolate* isolate, Handle<SharedFunctionInfo> shared_info);

  template <typename LocalIsolate>
  bool AreSourcePositionsAvailable(LocalIsolate* isolate) const;

  // Hash based on function literal id and script id.
  V8_EXPORT_PRIVATE uint32_t Hash();

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
#ifdef VERIFY_HEAP
  void SharedFunctionInfoVerify(LocalIsolate* isolate);
#endif
#ifdef OBJECT_PRINT
  void PrintSourceCode(std::ostream& os);
#endif

  // Iterate over all shared function infos in a given script.
  class ScriptIterator {
   public:
    V8_EXPORT_PRIVATE ScriptIterator(Isolate* isolate, Script script);
    explicit ScriptIterator(Handle<WeakFixedArray> shared_function_infos);
    ScriptIterator(const ScriptIterator&) = delete;
    ScriptIterator& operator=(const ScriptIterator&) = delete;
    V8_EXPORT_PRIVATE SharedFunctionInfo Next();
    int CurrentIndex() const { return index_ - 1; }

    // Reset the iterator to run on |script|.
    void Reset(Isolate* isolate, Script script);

   private:
    Handle<WeakFixedArray> shared_function_infos_;
    int index_;
  };

  // Constants.
  static const int kMaximumFunctionTokenOffset = kMaxUInt16 - 1;
  static const uint16_t kFunctionTokenOutOfRange = static_cast<uint16_t>(-1);
  STATIC_ASSERT(kMaximumFunctionTokenOffset + 1 == kFunctionTokenOutOfRange);

  static const int kAlignedSize = OBJECT_POINTER_ALIGN(kSize);

  class BodyDescriptor;

  // Bailout reasons must fit in the DisabledOptimizationReason bitfield.
  STATIC_ASSERT(BailoutReason::kLastErrorMessage <=
                DisabledOptimizationReasonBits::kMax);

  STATIC_ASSERT(kLastFunctionKind <= FunctionKindBits::kMax);
  STATIC_ASSERT(FunctionSyntaxKind::kLastFunctionSyntaxKind <=
                FunctionSyntaxKindBits::kMax);

  // Sets the bytecode in {shared}'s DebugInfo as the bytecode to
  // be returned by following calls to GetActiveBytecodeArray. Stores a
  // reference to the original bytecode in the DebugInfo.
  static void InstallDebugBytecode(Handle<SharedFunctionInfo> shared,
                                   Isolate* isolate);
  // Removes the debug bytecode and restores the original bytecode to be
  // returned by following calls to GetActiveBytecodeArray.
  static void UninstallDebugBytecode(SharedFunctionInfo shared,
                                     Isolate* isolate);

 private:
  friend class WebSnapshotDeserializer;

#ifdef VERIFY_HEAP
  void SharedFunctionInfoVerify(ReadOnlyRoots roots);
#endif

  // [name_or_scope_info]: Function name string, kNoSharedNameSentinel or
  // ScopeInfo.
  DECL_RELEASE_ACQUIRE_ACCESSORS(name_or_scope_info, Object)

  // [outer scope info] The outer scope info, needed to lazily parse this
  // function.
  DECL_ACCESSORS(outer_scope_info, HeapObject)

  // [is_oneshot_iife_or_properties_are_final]: This bit is used to track
  // two mutually exclusive cases. Either this SharedFunctionInfo is
  // a oneshot_iife or we have finished parsing its properties. These cases
  // are mutually exclusive because the properties final bit is only used by
  // class constructors to handle lazily parsed properties and class
  // constructors can never be oneshot iifes.
  DECL_BOOLEAN_ACCESSORS(is_oneshot_iife_or_properties_are_final)

  inline void set_kind(FunctionKind kind);

  inline uint16_t get_property_estimate_from_literal(FunctionLiteral* literal);

  template <typename Impl>
  friend class FactoryBase;
  friend class V8HeapExplorer;
  FRIEND_TEST(PreParserTest, LazyFunctionLength);

  TQ_OBJECT_CONSTRUCTORS(SharedFunctionInfo)
};

// Printing support.
struct SourceCodeOf {
  explicit SourceCodeOf(SharedFunctionInfo v, int max = -1)
      : value(v), max_length(max) {}
  const SharedFunctionInfo value;
  int max_length;
};

// IsCompiledScope enables a caller to check if a function is compiled, and
// ensure it remains compiled (i.e., doesn't have it's bytecode flushed) while
// the scope is retained.
class V8_NODISCARD IsCompiledScope {
 public:
  inline IsCompiledScope(const SharedFunctionInfo shared, Isolate* isolate);
  inline IsCompiledScope(const SharedFunctionInfo shared,
                         LocalIsolate* isolate);
  inline IsCompiledScope() : retain_bytecode_(), is_compiled_(false) {}

  inline bool is_compiled() const { return is_compiled_; }

 private:
  MaybeHandle<BytecodeArray> retain_bytecode_;
  bool is_compiled_;
};

std::ostream& operator<<(std::ostream& os, const SourceCodeOf& v);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SHARED_FUNCTION_INFO_H_
