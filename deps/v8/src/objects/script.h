// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SCRIPT_H_
#define V8_OBJECTS_SCRIPT_H_

#include "src/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

// Script describes a script which has been added to the VM.
class Script : public Struct {
 public:
  // Script types.
  enum Type {
    TYPE_NATIVE = 0,
    TYPE_EXTENSION = 1,
    TYPE_NORMAL = 2,
    TYPE_WASM = 3,
    TYPE_INSPECTOR = 4
  };

  // Script compilation types.
  enum CompilationType { COMPILATION_TYPE_HOST = 0, COMPILATION_TYPE_EVAL = 1 };

  // Script compilation state.
  enum CompilationState {
    COMPILATION_STATE_INITIAL = 0,
    COMPILATION_STATE_COMPILED = 1
  };

  // [source]: the script source.
  DECL_ACCESSORS(source, Object)

  // [name]: the script name.
  DECL_ACCESSORS(name, Object)

  // [id]: the script id.
  DECL_INT_ACCESSORS(id)

  // [line_offset]: script line offset in resource from where it was extracted.
  DECL_INT_ACCESSORS(line_offset)

  // [column_offset]: script column offset in resource from where it was
  // extracted.
  DECL_INT_ACCESSORS(column_offset)

  // [context_data]: context data for the context this script was compiled in.
  DECL_ACCESSORS(context_data, Object)

  // [wrapper]: the wrapper cache.  This is either undefined or a WeakCell.
  DECL_ACCESSORS(wrapper, HeapObject)

  // [type]: the script type.
  DECL_INT_ACCESSORS(type)

  // [line_ends]: FixedArray of line ends positions.
  DECL_ACCESSORS(line_ends, Object)

  // [eval_from_shared]: for eval scripts the shared function info for the
  // function from which eval was called.
  DECL_ACCESSORS(eval_from_shared, Object)

  // [eval_from_position]: the source position in the code for the function
  // from which eval was called, as positive integer. Or the code offset in the
  // code from which eval was called, as negative integer.
  DECL_INT_ACCESSORS(eval_from_position)

  // [shared_function_infos]: weak fixed array containing all shared
  // function infos created from this script.
  DECL_ACCESSORS(shared_function_infos, FixedArray)

  // [flags]: Holds an exciting bitfield.
  DECL_INT_ACCESSORS(flags)

  // [source_url]: sourceURL from magic comment
  DECL_ACCESSORS(source_url, Object)

  // [source_mapping_url]: sourceMappingURL magic comment
  DECL_ACCESSORS(source_mapping_url, Object)

  // [wasm_compiled_module]: the compiled wasm module this script belongs to.
  // This must only be called if the type of this script is TYPE_WASM.
  DECL_ACCESSORS(wasm_compiled_module, Object)

  // [host_defined_options]: Options defined by the embedder.
  DECL_ACCESSORS(host_defined_options, FixedArray)

  // [compilation_type]: how the the script was compiled. Encoded in the
  // 'flags' field.
  inline CompilationType compilation_type();
  inline void set_compilation_type(CompilationType type);

  // [compilation_state]: determines whether the script has already been
  // compiled. Encoded in the 'flags' field.
  inline CompilationState compilation_state();
  inline void set_compilation_state(CompilationState state);

  // [origin_options]: optional attributes set by the embedder via ScriptOrigin,
  // and used by the embedder to make decisions about the script. V8 just passes
  // this through. Encoded in the 'flags' field.
  inline v8::ScriptOriginOptions origin_options();
  inline void set_origin_options(ScriptOriginOptions origin_options);

  DECL_CAST(Script)

  // If script source is an external string, check that the underlying
  // resource is accessible. Otherwise, always return true.
  inline bool HasValidSource();

  Object* GetNameOrSourceURL();

  // Retrieve source position from where eval was called.
  int GetEvalPosition();

  // Init line_ends array with source code positions of line ends.
  static void InitLineEnds(Handle<Script> script);

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
  bool GetPositionInfo(int position, PositionInfo* info,
                       OffsetFlag offset_flag) const;

  bool IsUserJavaScript();

  // Wrappers for GetPositionInfo
  static int GetColumnNumber(Handle<Script> script, int code_offset);
  int GetColumnNumber(int code_pos) const;
  static int GetLineNumber(Handle<Script> script, int code_offset);
  int GetLineNumber(int code_pos) const;

  // Get the JS object wrapping the given script; create it if none exists.
  static Handle<JSObject> GetWrapper(Handle<Script> script);

  // Look through the list of existing shared function infos to find one
  // that matches the function literal.  Return empty handle if not found.
  MaybeHandle<SharedFunctionInfo> FindSharedFunctionInfo(
      Isolate* isolate, const FunctionLiteral* fun);

  // Iterate over all script objects on the heap.
  class Iterator {
   public:
    explicit Iterator(Isolate* isolate);
    Script* Next();

   private:
    WeakFixedArray::Iterator iterator_;
    DISALLOW_COPY_AND_ASSIGN(Iterator);
  };

  // Dispatched behavior.
  DECL_PRINTER(Script)
  DECL_VERIFIER(Script)

  static const int kSourceOffset = HeapObject::kHeaderSize;
  static const int kNameOffset = kSourceOffset + kPointerSize;
  static const int kLineOffsetOffset = kNameOffset + kPointerSize;
  static const int kColumnOffsetOffset = kLineOffsetOffset + kPointerSize;
  static const int kContextOffset = kColumnOffsetOffset + kPointerSize;
  static const int kWrapperOffset = kContextOffset + kPointerSize;
  static const int kTypeOffset = kWrapperOffset + kPointerSize;
  static const int kLineEndsOffset = kTypeOffset + kPointerSize;
  static const int kIdOffset = kLineEndsOffset + kPointerSize;
  static const int kEvalFromSharedOffset = kIdOffset + kPointerSize;
  static const int kEvalFromPositionOffset =
      kEvalFromSharedOffset + kPointerSize;
  static const int kSharedFunctionInfosOffset =
      kEvalFromPositionOffset + kPointerSize;
  static const int kFlagsOffset = kSharedFunctionInfosOffset + kPointerSize;
  static const int kSourceUrlOffset = kFlagsOffset + kPointerSize;
  static const int kSourceMappingUrlOffset = kSourceUrlOffset + kPointerSize;
  static const int kHostDefinedOptionsOffset =
      kSourceMappingUrlOffset + kPointerSize;
  static const int kSize = kHostDefinedOptionsOffset + kPointerSize;

 private:
  // Bit positions in the flags field.
  static const int kCompilationTypeBit = 0;
  static const int kCompilationStateBit = 1;
  static const int kOriginOptionsShift = 2;
  static const int kOriginOptionsSize = 4;
  static const int kOriginOptionsMask = ((1 << kOriginOptionsSize) - 1)
                                        << kOriginOptionsShift;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Script);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SCRIPT_H_
