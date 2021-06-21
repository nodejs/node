// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCRIPT_H_
#define V8_OBJECTS_SCRIPT_H_

#include <memory>

#include "src/base/export-template.h"
#include "src/objects/fixed-array.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {

namespace internal {

#include "torque-generated/src/objects/script-tq.inc"

// Script describes a script which has been added to the VM.
class Script : public TorqueGeneratedScript<Script, Struct> {
 public:
  // Script ID used for temporary scripts, which shouldn't be added to the
  // script list.
  static constexpr int kTemporaryScriptId = -2;

  NEVER_READ_ONLY_SPACE
  // Script types.
  enum Type {
    TYPE_NATIVE = 0,
    TYPE_EXTENSION = 1,
    TYPE_NORMAL = 2,
#if V8_ENABLE_WEBASSEMBLY
    TYPE_WASM = 3,
#endif  // V8_ENABLE_WEBASSEMBLY
    TYPE_INSPECTOR = 4
  };

  // Script compilation types.
  enum CompilationType { COMPILATION_TYPE_HOST = 0, COMPILATION_TYPE_EVAL = 1 };

  // Script compilation state.
  enum CompilationState {
    COMPILATION_STATE_INITIAL = 0,
    COMPILATION_STATE_COMPILED = 1
  };

  // [type]: the script type.
  DECL_INT_ACCESSORS(type)

  DECL_ACCESSORS(eval_from_shared_or_wrapped_arguments, Object)

  // [eval_from_shared]: for eval scripts the shared function info for the
  // function from which eval was called.
  DECL_ACCESSORS(eval_from_shared, SharedFunctionInfo)

  // [wrapped_arguments]: for the list of arguments in a wrapped script.
  DECL_ACCESSORS(wrapped_arguments, FixedArray)

  // Whether the script is implicitly wrapped in a function.
  inline bool is_wrapped() const;

  // Whether the eval_from_shared field is set with a shared function info
  // for the eval site.
  inline bool has_eval_from_shared() const;

  // [eval_from_position]: the source position in the code for the function
  // from which eval was called, as positive integer. Or the code offset in the
  // code from which eval was called, as negative integer.
  DECL_INT_ACCESSORS(eval_from_position)

  // [shared_function_infos]: weak fixed array containing all shared
  // function infos created from this script.
  DECL_ACCESSORS(shared_function_infos, WeakFixedArray)

#if V8_ENABLE_WEBASSEMBLY
  // [wasm_breakpoint_infos]: the list of {BreakPointInfo} objects describing
  // all WebAssembly breakpoints for modules/instances managed via this script.
  // This must only be called if the type of this script is TYPE_WASM.
  DECL_ACCESSORS(wasm_breakpoint_infos, FixedArray)
  inline bool has_wasm_breakpoint_infos() const;

  // [wasm_native_module]: the wasm {NativeModule} this script belongs to.
  // This must only be called if the type of this script is TYPE_WASM.
  DECL_ACCESSORS(wasm_managed_native_module, Object)
  inline wasm::NativeModule* wasm_native_module() const;

  // [wasm_weak_instance_list]: the list of all {WasmInstanceObject} being
  // affected by breakpoints that are managed via this script.
  // This must only be called if the type of this script is TYPE_WASM.
  DECL_ACCESSORS(wasm_weak_instance_list, WeakArrayList)

  // [break_on_entry] (wasm only): whether an instrumentation breakpoint is set
  // for this script; this information will be transferred to existing and
  // future instances to make sure that we stop before executing any code in
  // this wasm module.
  inline bool break_on_entry() const;
  inline void set_break_on_entry(bool value);

  // Check if the script contains any Asm modules.
  bool ContainsAsmModule();
#endif  // V8_ENABLE_WEBASSEMBLY

  // [compilation_type]: how the the script was compiled. Encoded in the
  // 'flags' field.
  inline CompilationType compilation_type();
  inline void set_compilation_type(CompilationType type);

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

  // If script source is an external string, check that the underlying
  // resource is accessible. Otherwise, always return true.
  inline bool HasValidSource();

  Object GetNameOrSourceURL();

  // Retrieve source position from where eval was called.
  static int GetEvalPosition(Isolate* isolate, Handle<Script> script);

  // Init line_ends array with source code positions of line ends.
  template <typename LocalIsolate>
  EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
  static void InitLineEnds(LocalIsolate* isolate, Handle<Script> script);

  // Carries information about a source position.
  struct PositionInfo {
    PositionInfo() : line(-1), column(-1), line_start(-1), line_end(-1) {}

    int line;        // Zero-based line number.
    int column;      // Zero-based column number.
    int line_start;  // Position of first character in line.
    int line_end;    // Position of final linebreak character in line.
  };

  // Specifies whether to add offsets to position infos.
  enum OffsetFlag { NO_OFFSET = 0, WITH_OFFSET = 1 };

  // Retrieves information about the given position, optionally with an offset.
  // Returns false on failure, and otherwise writes into the given info object
  // on success.
  // The static method should is preferable for handlified callsites because it
  // initializes the line ends array, avoiding expensive recomputations.
  // The non-static version is not allocating and safe for unhandlified
  // callsites.
  static bool GetPositionInfo(Handle<Script> script, int position,
                              PositionInfo* info, OffsetFlag offset_flag);
  V8_EXPORT_PRIVATE bool GetPositionInfo(int position, PositionInfo* info,
                                         OffsetFlag offset_flag) const;

  bool IsUserJavaScript() const;

  // Wrappers for GetPositionInfo
  static int GetColumnNumber(Handle<Script> script, int code_offset);
  int GetColumnNumber(int code_pos) const;
  V8_EXPORT_PRIVATE static int GetLineNumber(Handle<Script> script,
                                             int code_offset);
  int GetLineNumber(int code_pos) const;

  // Look through the list of existing shared function infos to find one
  // that matches the function literal.  Return empty handle if not found.
  template <typename LocalIsolate>
  MaybeHandle<SharedFunctionInfo> FindSharedFunctionInfo(
      LocalIsolate* isolate, int function_literal_id);

  // Iterate over all script objects on the heap.
  class V8_EXPORT_PRIVATE Iterator {
   public:
    explicit Iterator(Isolate* isolate);
    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;
    Script Next();

   private:
    WeakArrayList::Iterator iterator_;
  };

  // Dispatched behavior.
  DECL_PRINTER(Script)
  DECL_VERIFIER(Script)

 private:
  // Bit positions in the flags field.
  DEFINE_TORQUE_GENERATED_SCRIPT_FLAGS()

  TQ_OBJECT_CONSTRUCTORS(Script)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCRIPT_H_
