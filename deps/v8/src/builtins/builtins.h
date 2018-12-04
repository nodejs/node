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
                 DEF_ENUM, DEF_ENUM, DEF_ENUM)
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
  enum Kind { CPP, API, TFJ, TFC, TFS, TFH, BCH, DLH, ASM };

  static BailoutId GetContinuationBailoutId(Name name);
  static Name GetBuiltinFromBailoutId(BailoutId);

  // Convenience wrappers.
  Handle<Code> CallFunction(ConvertReceiverMode = ConvertReceiverMode::kAny);
  Handle<Code> Call(ConvertReceiverMode = ConvertReceiverMode::kAny);
  Handle<Code> NonPrimitiveToPrimitive(
      ToPrimitiveHint hint = ToPrimitiveHint::kDefault);
  Handle<Code> OrdinaryToPrimitive(OrdinaryToPrimitiveHint hint);
  Handle<Code> InterpreterPushArgsThenCall(ConvertReceiverMode receiver_mode,
                                           InterpreterPushArgsMode mode);
  Handle<Code> InterpreterPushArgsThenConstruct(InterpreterPushArgsMode mode);
  Handle<Code> NewFunctionContext(ScopeType scope_type);
  Handle<Code> JSConstructStubGeneric();

  // Used by BuiltinDeserializer and CreateOffHeapTrampolines in isolate.cc.
  void set_builtin(int index, HeapObject* builtin);

  Code* builtin(int index);
  V8_EXPORT_PRIVATE Handle<Code> builtin_handle(int index);

  V8_EXPORT_PRIVATE static Callable CallableFor(Isolate* isolate, Name name);

  static int GetStackParameterCount(Name name);

  static const char* name(int index);

  // Returns the C++ entry point for builtins implemented in C++, and the null
  // Address otherwise.
  static Address CppEntryOf(int index);

  static Kind KindOf(int index);
  static const char* KindNameOf(int index);

  static bool IsCpp(int index);
  static bool HasCppImplementation(int index);

  // True, iff the given code object is a builtin. Note that this does not
  // necessarily mean that its kind is Code::BUILTIN.
  static bool IsBuiltin(const Code* code);

  // As above, but safe to access off the main thread since the check is done
  // by handle location. Similar to Heap::IsRootHandle.
  bool IsBuiltinHandle(Handle<HeapObject> maybe_code, int* index) const;

  // True, iff the given code object is a builtin with off-heap embedded code.
  static bool IsIsolateIndependentBuiltin(const Code* code);

  // Returns true iff the given builtin can be lazy-loaded from the snapshot.
  // This is true in general for most builtins with the exception of a few
  // special cases such as CompileLazy and DeserializeLazy.
  static bool IsLazy(int index);

  static constexpr int kFirstWideBytecodeHandler =
      kFirstBytecodeHandler + kNumberOfBytecodeHandlers;
  static constexpr int kFirstExtraWideBytecodeHandler =
      kFirstWideBytecodeHandler + kNumberOfWideBytecodeHandlers;
  STATIC_ASSERT(kFirstExtraWideBytecodeHandler +
                    kNumberOfWideBytecodeHandlers ==
                builtin_count);

  // Returns the index of the appropriate lazy deserializer in the builtins
  // table.
  static constexpr int LazyDeserializerForBuiltin(const int index) {
    return index < kFirstWideBytecodeHandler
               ? (index < kFirstBytecodeHandler
                      ? Builtins::kDeserializeLazy
                      : Builtins::kDeserializeLazyHandler)
               : (index < kFirstExtraWideBytecodeHandler
                      ? Builtins::kDeserializeLazyWideHandler
                      : Builtins::kDeserializeLazyExtraWideHandler);
  }

  static constexpr bool IsLazyDeserializer(int builtin_index) {
    return builtin_index == kDeserializeLazy ||
           builtin_index == kDeserializeLazyHandler ||
           builtin_index == kDeserializeLazyWideHandler ||
           builtin_index == kDeserializeLazyExtraWideHandler;
  }

  static bool IsLazyDeserializer(Code* code);

  // Helper methods used for testing isolate-independent builtins.
  // TODO(jgruber,v8:6666): Remove once all builtins have been migrated.
  static bool IsIsolateIndependent(int index);

  // Wasm runtime stubs are treated specially by wasm. To guarantee reachability
  // through near jumps, their code is completely copied into a fresh off-heap
  // area.
  static bool IsWasmRuntimeStub(int index);

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
               DECLARE_TF, DECLARE_TF, IGNORE_BUILTIN, IGNORE_BUILTIN,
               DECLARE_ASM)

#undef DECLARE_ASM
#undef DECLARE_TF

  Isolate* isolate_;
  bool initialized_ = false;

  friend class SetupIsolateDelegate;

  DISALLOW_COPY_AND_ASSIGN(Builtins);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_H_
