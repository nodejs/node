// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_H_
#define V8_BUILTINS_BUILTINS_H_

#include "src/base/flags.h"
#include "src/builtins/builtins-definitions.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class ByteArray;
class Callable;
template <typename T>
class Handle;
class Isolate;

// Forward declarations.
class BailoutId;
class RootVisitor;
enum class InterpreterPushArgsMode : unsigned;
namespace compiler {
class CodeAssemblerState;
}

template <typename T>
static constexpr T FirstFromVarArgs(T x, ...) noexcept {
  return x;
}

// Convenience macro to avoid generating named accessors for all builtins.
#define BUILTIN_CODE(isolate, name) \
  (isolate)->builtins()->builtin_handle(Builtins::k##name)

class Builtins {
 public:
  explicit Builtins(Isolate* isolate) : isolate_(isolate) {}

  void TearDown();

  // Disassembler support.
  const char* Lookup(Address pc);

  enum Name : int32_t {
#define DEF_ENUM(Name, ...) k##Name,
    BUILTIN_LIST(DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM, DEF_ENUM,
                 DEF_ENUM, DEF_ENUM)
#undef DEF_ENUM
        builtin_count,

#define EXTRACT_NAME(Name, ...) k##Name,
    // Define kFirstBytecodeHandler,
    kFirstBytecodeHandler =
        FirstFromVarArgs(BUILTIN_LIST_BYTECODE_HANDLERS(EXTRACT_NAME) 0)
#undef EXTRACT_NAME
  };

  static const int32_t kNoBuiltinId = -1;

  static constexpr bool IsBuiltinId(int maybe_id) {
    return 0 <= maybe_id && maybe_id < builtin_count;
  }

  // The different builtin kinds are documented in builtins-definitions.h.
  enum Kind { CPP, API, TFJ, TFC, TFS, TFH, BCH, ASM };

  static BailoutId GetContinuationBailoutId(Name name);
  static Name GetBuiltinFromBailoutId(BailoutId);

  // Convenience wrappers.
  Handle<Code> CallFunction(ConvertReceiverMode = ConvertReceiverMode::kAny);
  Handle<Code> Call(ConvertReceiverMode = ConvertReceiverMode::kAny);
  Handle<Code> NonPrimitiveToPrimitive(
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);
  Handle<Code> OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint);
  Handle<Code> JSConstructStubGeneric();

  // Used by CreateOffHeapTrampolines in isolate.cc.
  void set_builtin(int index, Code builtin);

  Code builtin(int index);
  V8_EXPORT_PRIVATE Handle<Code> builtin_handle(int index);

  V8_EXPORT_PRIVATE static Callable CallableFor(Isolate* isolate, Name name);

  static int GetStackParameterCount(Name name);

  static const char* name(int index);

  // Support for --print-builtin-size and --print-builtin-code.
  void PrintBuiltinCode();
  void PrintBuiltinSize();

  // Returns the C++ entry point for builtins implemented in C++, and the null
  // Address otherwise.
  static Address CppEntryOf(int index);

  static Kind KindOf(int index);
  static const char* KindNameOf(int index);

  static bool IsCpp(int index);
  static bool HasCppImplementation(int index);

  // True, iff the given code object is a builtin. Note that this does not
  // necessarily mean that its kind is Code::BUILTIN.
  static bool IsBuiltin(const Code code);

  // As above, but safe to access off the main thread since the check is done
  // by handle location. Similar to Heap::IsRootHandle.
  bool IsBuiltinHandle(Handle<HeapObject> maybe_code, int* index) const;

  // True, iff the given code object is a builtin with off-heap embedded code.
  static bool IsIsolateIndependentBuiltin(const Code code);

  static constexpr int kFirstWideBytecodeHandler =
      kFirstBytecodeHandler + kNumberOfBytecodeHandlers;
  static constexpr int kFirstExtraWideBytecodeHandler =
      kFirstWideBytecodeHandler + kNumberOfWideBytecodeHandlers;
  STATIC_ASSERT(kFirstExtraWideBytecodeHandler +
                    kNumberOfWideBytecodeHandlers ==
                builtin_count);

  // True, iff the given builtin contains no isolate-specific code and can be
  // embedded into the binary.
  static constexpr bool kAllBuiltinsAreIsolateIndependent = true;
  static constexpr bool AllBuiltinsAreIsolateIndependent() {
    return kAllBuiltinsAreIsolateIndependent;
  }
  static constexpr bool IsIsolateIndependent(int index) {
    STATIC_ASSERT(kAllBuiltinsAreIsolateIndependent);
    return kAllBuiltinsAreIsolateIndependent;
  }

  // Wasm runtime stubs are treated specially by wasm. To guarantee reachability
  // through near jumps, their code is completely copied into a fresh off-heap
  // area.
  static bool IsWasmRuntimeStub(int index);

  // Updates the table of builtin entry points based on the current contents of
  // the builtins table.
  static void UpdateBuiltinEntryTable(Isolate* isolate);

  bool is_initialized() const { return initialized_; }

  // Used by SetupIsolateDelegate and Deserializer.
  void MarkInitialized() {
    DCHECK(!initialized_);
    initialized_ = true;
  }

  V8_WARN_UNUSED_RESULT static MaybeHandle<Object> InvokeApiFunction(
      Isolate* isolate, bool is_construct, Handle<HeapObject> function,
      Handle<Object> receiver, int argc, Handle<Object> args[],
      Handle<HeapObject> new_target);

  enum ExitFrameType { EXIT, BUILTIN_EXIT };

  static void Generate_Adaptor(MacroAssembler* masm, Address builtin_address,
                               ExitFrameType exit_frame_type);

  static void Generate_CEntry(MacroAssembler* masm, int result_size,
                              SaveFPRegsMode save_doubles, ArgvMode argv_mode,
                              bool builtin_exit_frame);

  static bool AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                   Handle<JSObject> target_global_proxy);

  // Creates a trampoline code object that jumps to the given off-heap entry.
  // The result should not be used directly, but only from the related Factory
  // function.
  static Handle<Code> GenerateOffHeapTrampolineFor(Isolate* isolate,
                                                   Address off_heap_entry);

  // Generate the RelocInfo ByteArray that would be generated for an offheap
  // trampoline.
  static Handle<ByteArray> GenerateOffHeapTrampolineRelocInfo(Isolate* isolate);

  static bool IsJSEntryVariant(int builtin_index) {
    switch (builtin_index) {
      case kJSEntry:
      case kJSConstructEntry:
      case kJSRunMicrotasksEntry:
        return true;
      default:
        return false;
    }
    UNREACHABLE();
  }

  int js_entry_handler_offset() const {
    DCHECK_NE(js_entry_handler_offset_, 0);
    return js_entry_handler_offset_;
  }

  void SetJSEntryHandlerOffset(int offset) {
    // Check the stored offset is either uninitialized or unchanged (we
    // generate multiple variants of this builtin but they should all have the
    // same handler offset).
    CHECK(js_entry_handler_offset_ == 0 || js_entry_handler_offset_ == offset);
    js_entry_handler_offset_ = offset;
  }

 private:
  static void Generate_CallFunction(MacroAssembler* masm,
                                    ConvertReceiverMode mode);

  static void Generate_CallBoundFunctionImpl(MacroAssembler* masm);

  static void Generate_Call(MacroAssembler* masm, ConvertReceiverMode mode);

  enum class CallOrConstructMode { kCall, kConstruct };
  static void Generate_CallOrConstructVarargs(MacroAssembler* masm,
                                              Handle<Code> code);
  static void Generate_CallOrConstructForwardVarargs(MacroAssembler* masm,
                                                     CallOrConstructMode mode,
                                                     Handle<Code> code);

  static void Generate_InterpreterPushArgsThenCallImpl(
      MacroAssembler* masm, ConvertReceiverMode receiver_mode,
      InterpreterPushArgsMode mode);

  static void Generate_InterpreterPushArgsThenConstructImpl(
      MacroAssembler* masm, InterpreterPushArgsMode mode);

#define DECLARE_ASM(Name, ...) \
  static void Generate_##Name(MacroAssembler* masm);
#define DECLARE_TF(Name, ...) \
  static void Generate_##Name(compiler::CodeAssemblerState* state);

  BUILTIN_LIST(IGNORE_BUILTIN, IGNORE_BUILTIN, DECLARE_TF, DECLARE_TF,
               DECLARE_TF, DECLARE_TF, IGNORE_BUILTIN, DECLARE_ASM)

#undef DECLARE_ASM
#undef DECLARE_TF

  Isolate* isolate_;
  bool initialized_ = false;

  // Stores the offset of exception handler entry point (the handler_entry
  // label) in JSEntry and its variants. It's used to generate the handler table
  // during codegen (mksnapshot-only).
  int js_entry_handler_offset_ = 0;

  friend class SetupIsolateDelegate;

  DISALLOW_COPY_AND_ASSIGN(Builtins);
};

Builtins::Name ExampleBuiltinForTorqueFunctionPointerType(
    size_t function_pointer_type_id);

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_H_
