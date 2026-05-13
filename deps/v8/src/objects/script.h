// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCRIPT_H_
#define V8_OBJECTS_SCRIPT_H_

#include <memory>

#include "include/v8-script.h"
#include "src/base/export-template.h"
#include "src/heap/factory-base.h"
#include "src/heap/factory.h"
#include "src/heap/local-factory.h"
#include "src/objects/fixed-array.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"
#include "src/objects/string.h"
#include "src/objects/struct.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {

namespace internal {

class FunctionLiteral;
class StructBodyDescriptor;

namespace wasm {
class NativeModule;
}  // namespace wasm

#include "torque-generated/src/objects/script-tq.inc"

// Script describes a script which has been added to the VM.
V8_OBJECT class Script : public Struct {
 public:
  // Script ID used for temporary scripts, which shouldn't be added to the
  // script list.
  static constexpr int kTemporaryScriptId = -2;

  // Script types.
  enum class Type {
    kNative = 0,
    kExtension = 1,
    kNormal = 2,
#if V8_ENABLE_WEBASSEMBLY
    kWasm = 3,
#endif  // V8_ENABLE_WEBASSEMBLY
    kInspector = 4
  };

  // Script compilation types.
  enum class CompilationType { kHost = 0, kEval = 1 };

  // Script compilation state.
  enum class CompilationState { kInitial = 0, kCompiled = 1 };

  // [source]: the script source.
  inline Tagged<UnionOf<String, Undefined>> source() const;
  inline void set_source(Tagged<UnionOf<String, Undefined>> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [name]: the script name.
  inline Tagged<Object> name() const;
  inline void set_name(Tagged<Object> value,
                       WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [line_offset]: script line offset in resource from where it was extracted.
  inline int line_offset() const;
  inline void set_line_offset(int value);

  // [column_offset]: script column offset in resource from where it was
  // extracted.
  inline int column_offset() const;
  inline void set_column_offset(int value);

  // [context_data]: context data for the context this script was compiled in.
  inline Tagged<UnionOf<Smi, Undefined, Symbol>> context_data() const;
  inline void set_context_data(Tagged<UnionOf<Smi, Undefined, Symbol>> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [type]: the script type.
  inline Type type() const;
  inline void set_type(Type value);

  // [line_ends]: FixedArray of line ends positions.
  inline Tagged<UnionOf<FixedArray, Smi>> line_ends() const;
  inline void set_line_ends(Tagged<UnionOf<FixedArray, Smi>> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [id]: the script id.
  inline int id() const;
  inline void set_id(int value);

  inline Tagged<Object> eval_from_shared_or_wrapped_arguments() const;
  inline void set_eval_from_shared_or_wrapped_arguments(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [eval_from_shared]: for eval scripts the shared function info for the
  // function from which eval was called.
  inline Tagged<SharedFunctionInfo> eval_from_shared() const;
  inline void set_eval_from_shared(
      Tagged<SharedFunctionInfo> shared,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [wrapped_arguments]: for the list of arguments in a wrapped script.
  inline Tagged<FixedArray> wrapped_arguments() const;
  inline void set_wrapped_arguments(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Whether the script is implicitly wrapped in a function.
  inline bool is_wrapped() const;

  // Whether the eval_from_shared field is set with a shared function info
  // for the eval site.
  inline bool has_eval_from_shared() const;

  // [eval_from_position]: the source position in the code for the function
  // from which eval was called, as positive integer. Or the code offset in the
  // code from which eval was called, as negative integer.
  inline int eval_from_position() const;
  inline void set_eval_from_position(int value);

  inline Tagged<Object> eval_from_scope_info() const;
  inline void set_eval_from_scope_info(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline bool has_eval_from_scope_info() const;

  // [infos]: weak fixed array containing all shared function infos and scope
  // infos for eval created from this script.
  inline Tagged<WeakFixedArray> infos() const;
  inline void set_infos(Tagged<WeakFixedArray> value,
                        WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

#if V8_ENABLE_WEBASSEMBLY
  // [wasm_breakpoint_infos]: the list of {BreakPointInfo} objects describing
  // all WebAssembly breakpoints for modules/instances managed via this script.
  // This must only be called if the type of this script is TYPE_WASM.
  inline Tagged<FixedArray> wasm_breakpoint_infos() const;
  inline void set_wasm_breakpoint_infos(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline bool has_wasm_breakpoint_infos() const;

  // [wasm_native_module]: the wasm {NativeModule} this script belongs to.
  // This must only be called if the type of this script is TYPE_WASM.
  inline Tagged<Object> wasm_managed_native_module() const;
  inline void set_wasm_managed_native_module(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Managed<wasm::NativeModule>::Ptr wasm_native_module() const;

  // [wasm_weak_instance_list]: the list of all {WasmInstanceObject} being
  // affected by breakpoints that are managed via this script.
  // This must only be called if the type of this script is TYPE_WASM.
  inline Tagged<WeakArrayList> wasm_weak_instance_list() const;
  inline void set_wasm_weak_instance_list(
      Tagged<WeakArrayList> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [break_on_entry] (wasm only): whether an instrumentation breakpoint is set
  // for this script; this information will be transferred to existing and
  // future instances to make sure that we stop before executing any code in
  // this wasm module.
  inline bool break_on_entry() const;
  inline void set_break_on_entry(bool value);

  // Check if the script contains any Asm modules.
  bool ContainsAsmModule();
#endif  // V8_ENABLE_WEBASSEMBLY

  // Read/write the raw 'flags' field. This uses relaxed atomic loads/stores
  // because the flags are read by background compile threads and updated by the
  // main thread.
  inline uint32_t flags() const;
  inline void set_flags(uint32_t new_flags);

  // [compilation_type]: how the script was compiled. Encoded in the
  // 'flags' field.
  inline CompilationType compilation_type() const;
  inline void set_compilation_type(CompilationType type);

  inline bool produce_compile_hints() const;
  inline void set_produce_compile_hints(bool produce_compile_hints);

  inline bool deserialized() const;
  inline void set_deserialized(bool value);

  // [compilation_state]: determines whether the script has already been
  // compiled. Encoded in the 'flags' field.
  inline CompilationState compilation_state();
  inline void set_compilation_state(CompilationState state);

  // [is_repl_mode]: whether this script originated from a REPL via debug
  // evaluate and therefore has different semantics, e.g. re-declaring let.
  inline bool is_repl_mode() const;
  inline void set_is_repl_mode(bool value);

  // [origin_options]: optional attributes set by the embedder via ScriptOrigin,
  // and used by the embedder to make decisions about the script. V8 just passes
  // this through. Encoded in the 'flags' field.
  inline v8::ScriptOriginOptions origin_options();
  inline void set_origin_options(ScriptOriginOptions origin_options);

  inline Tagged<UnionOf<ArrayList, Undefined>>
  compiled_lazy_function_positions() const;
  inline void set_compiled_lazy_function_positions(
      Tagged<UnionOf<ArrayList, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<String, Undefined>> source_url() const;
  inline void set_source_url(Tagged<UnionOf<String, Undefined>> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Object> source_mapping_url() const;
  inline void set_source_mapping_url(
      Tagged<Object> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<String, Undefined>> debug_id() const;
  inline void set_debug_id(Tagged<UnionOf<String, Undefined>> value,
                           WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> host_defined_options() const;
  inline void set_host_defined_options(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

#if V8_SCRIPTORMODULE_LEGACY_LIFETIME
  inline Tagged<ArrayList> script_or_modules() const;
  inline void set_script_or_modules(
      Tagged<ArrayList> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
#endif

  inline Tagged<UnionOf<String, Undefined>> source_hash() const;
  inline void set_source_hash(Tagged<UnionOf<String, Undefined>> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // If script source is an external string, check that the underlying
  // resource is accessible. Otherwise, always return true.
  inline bool HasValidSource();

  // If the script has a non-empty sourceURL comment.
  inline bool HasSourceURLComment() const;

  // If the script has a non-empty sourceMappingURL comment.
  inline bool HasSourceMappingURLComment() const;

  // Streaming compilation only attaches the source to the Script upon
  // finalization. This predicate returns true, if this script may still be
  // unfinalized.
  inline bool IsMaybeUnfinalized(Isolate* isolate) const;

  Tagged<Object> GetNameOrSourceURL();
  static DirectHandle<String> GetScriptHash(Isolate* isolate,
                                            DirectHandle<Script> script,
                                            bool forceForInspector);

  // Retrieve source position from where eval was called.
  static int GetEvalPosition(Isolate* isolate, DirectHandle<Script> script);

  Tagged<Script> inline GetEvalOrigin();

  // Initialize line_ends array with source code positions of line ends if
  // it doesn't exist yet.
  static inline void InitLineEnds(Isolate* isolate,
                                  DirectHandle<Script> script);
  static inline void InitLineEnds(LocalIsolate* isolate,
                                  DirectHandle<Script> script);

  // Obtain line ends as a vector, without modifying the script object
  V8_EXPORT_PRIVATE static String::LineEndsVector GetLineEnds(
      Isolate* isolate, DirectHandle<Script> script);

  inline bool has_line_ends() const;

  // Will initialize the line ends if required.
  static void SetSource(Isolate* isolate, DirectHandle<Script> script,
                        DirectHandle<String> source);

  bool inline CanHaveLineEnds() const;

  // Carries information about a source position.
  struct PositionInfo {
    PositionInfo() : line(-1), column(-1), line_start(-1), line_end(-1) {}

    int line;        // Zero-based line number.
    int column;      // Zero-based column number.
    int line_start;  // Position of first character in line.
    int line_end;    // Position of final linebreak character in line.
  };

  // Specifies whether to add offsets to position infos.
  enum class OffsetFlag { kNoOffset, kWithOffset };

  // Retrieves information about the given position, optionally with an offset.
  // Returns false on failure, and otherwise writes into the given info object
  // on success.
  // The static method should is preferable for handlified callsites because it
  // initializes the line ends array, avoiding expensive recomputations.
  // The non-static version is not allocating and safe for unhandlified
  // callsites.
  static bool GetPositionInfo(DirectHandle<Script> script, int position,
                              PositionInfo* info,
                              OffsetFlag offset_flag = OffsetFlag::kWithOffset);
  static bool GetLineColumnWithLineEnds(
      int position, int& line, int& column,
      const String::LineEndsVector& line_ends);
  V8_EXPORT_PRIVATE bool GetPositionInfo(
      int position, PositionInfo* info,
      OffsetFlag offset_flag = OffsetFlag::kWithOffset) const;
  V8_EXPORT_PRIVATE bool GetPositionInfoWithLineEnds(
      int position, PositionInfo* info, const String::LineEndsVector& line_ends,
      OffsetFlag offset_flag = OffsetFlag::kWithOffset) const;
  V8_EXPORT_PRIVATE void AddPositionInfoOffset(
      PositionInfo* info,
      OffsetFlag offset_flag = OffsetFlag::kWithOffset) const;

  // Tells whether this script should be subject to debugging, e.g. for
  // - scope inspection
  // - internal break points
  // - coverage and type profile
  // - error stack trace
  bool IsSubjectToDebugging() const;

  bool IsUserJavaScript() const;

  void TraceScriptRundown();
  void TraceScriptRundownSources();

  // Wrappers for GetPositionInfo
  static int GetColumnNumber(DirectHandle<Script> script, int code_offset);
  int GetColumnNumber(int code_pos) const;
  V8_EXPORT_PRIVATE static int GetLineNumber(DirectHandle<Script> script,
                                             int code_offset);
  int GetLineNumber(int code_pos) const;

  // Look through the list of existing shared function infos to find one
  // that matches the function literal. Return empty handle if not found.
  template <typename IsolateT>
  static MaybeHandle<SharedFunctionInfo> FindSharedFunctionInfo(
      DirectHandle<Script> script, IsolateT* isolate,
      FunctionLiteral* function_literal);

  // Iterate over all script objects on the heap.
  class V8_EXPORT_PRIVATE Iterator {
   public:
    explicit Iterator(Isolate* isolate);
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Tagged<Script> Next();

   private:
    WeakArrayList::Iterator iterator_;
  };

  // Dispatched behavior.
  DECL_PRINTER(Script)
  DECL_VERIFIER(Script)

  using BodyDescriptor = StructBodyDescriptor;

 public:
  TaggedMember<UnionOf<String, Undefined>> source_;
  TaggedMember<Object> name_;
  TaggedMember<Smi> line_offset_;
  TaggedMember<Smi> column_offset_;
  TaggedMember<UnionOf<Smi, Undefined, Symbol>> context_data_;
  TaggedMember<Smi> script_type_;
  TaggedMember<UnionOf<FixedArray, Smi>> line_ends_;
  TaggedMember<Smi> id_;
  TaggedMember<Object> eval_from_shared_or_wrapped_arguments_;
  TaggedMember<UnionOf<Smi, Foreign>> eval_from_position_;
  TaggedMember<UnionOf<ScopeInfo, Undefined>> eval_from_scope_info_;
  TaggedMember<UnionOf<WeakFixedArray, WeakArrayList>> infos_;
  TaggedMember<UnionOf<ArrayList, Undefined>> compiled_lazy_function_positions_;
  TaggedMember<Smi> flags_;
  TaggedMember<UnionOf<String, Undefined>> source_url_;
  TaggedMember<Object> source_mapping_url_;
  TaggedMember<UnionOf<String, Undefined>> debug_id_;
  TaggedMember<FixedArray> host_defined_options_;
#if V8_SCRIPTORMODULE_LEGACY_LIFETIME
  TaggedMember<ArrayList> script_or_modules_;
#endif
  TaggedMember<UnionOf<String, Undefined>> source_hash_;

 private:
  template <typename LineEndsContainer>
  bool GetPositionInfoInternal(const LineEndsContainer& ends, int position,
                               Script::PositionInfo* info,
                               const DisallowGarbageCollection& no_gc) const;

  friend Factory;
  friend FactoryBase<Factory>;
  friend FactoryBase<LocalFactory>;

  // Bit positions in the flags field.
  DEFINE_TORQUE_GENERATED_SCRIPT_FLAGS()

  template <typename IsolateT>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  static void V8_PRESERVE_MOST
      InitLineEndsInternal(IsolateT* isolate, DirectHandle<Script> script);
} V8_OBJECT_END;

V8_EXPORT_PRIVATE const char* ToString(Script::Type type);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCRIPT_H_
