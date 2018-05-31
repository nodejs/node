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

// Convenience macro to avoid generating named accessors for all builtins.
#define BUILTIN_CODE(isolate, name) \
  (isolate)->builtins()->builtin_handle(Builtins::k##name)

class Builtins {
 public:
  ~Builtins();

  void TearDown();

  // Garbage collection support.
  void IterateBuiltins(RootVisitor* v);

  // Disassembler support.
  const char* Lookup(byte* pc);

  enum Name : int32_t {
#define DEF_ENUM(Name, ...) k##Name,
    BUILTIN_LIST_ALL(DEF_ENUM)
#undef DEF_ENUM
        builtin_count
  };

  static const int32_t kNoBuiltinId = -1;

  static bool IsBuiltinId(int maybe_id) {
    return 0 <= maybe_id && maybe_id < builtin_count;
  }

  // The different builtin kinds are documented in builtins-definitions.h.
  enum Kind { CPP, API, TFJ, TFC, TFS, TFH, ASM };

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

  Code* builtin(int index) {
    DCHECK(IsBuiltinId(index));
    // Code::cast cannot be used here since we access builtins
    // during the marking phase of mark sweep. See IC::Clear.
    return reinterpret_cast<Code*>(builtins_[index]);
  }

  Address builtin_address(int index) {
    DCHECK(IsBuiltinId(index));
    return reinterpret_cast<Address>(&builtins_[index]);
  }

  V8_EXPORT_PRIVATE Handle<Code> builtin_handle(int index);

  // Used by lazy deserialization to determine whether a given builtin has been
  // deserialized. See the DeserializeLazy builtin.
  Object** builtins_table_address() { return &builtins_[0]; }

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

  // True, iff the given code object is a builtin with off-heap embedded code.
  static bool IsEmbeddedBuiltin(const Code* code);

  // Returns true iff the given builtin can be lazy-loaded from the snapshot.
  // This is true in general for most builtins with the exception of a few
  // special cases such as CompileLazy and DeserializeLazy.
  static bool IsLazy(int index);

  // Helper methods used for testing isolate-independent builtins.
  // TODO(jgruber,v8:6666): Remove once all builtins have been migrated.
  static bool IsIsolateIndependent(int index);

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

  static bool AllowDynamicFunction(Isolate* isolate, Handle<JSFunction> target,
                                   Handle<JSObject> target_global_proxy);

 private:
  Builtins();

#ifdef V8_EMBEDDED_BUILTINS
  // Creates a trampoline code object that jumps to the given off-heap entry.
  // The result should not be used directly, but only from the related Factory
  // function.
  static Handle<Code> GenerateOffHeapTrampolineFor(Isolate* isolate,
                                                   Address off_heap_entry);
#endif

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
               DECLARE_TF, DECLARE_TF, DECLARE_ASM)

#undef DECLARE_ASM
#undef DECLARE_TF

  // Note: These are always Code objects, but to conform with
  // IterateBuiltins() above which assumes Object**'s for the callback
  // function f, we use an Object* array here.
  Object* builtins_[builtin_count];
  bool initialized_;

  friend class Factory;  // For GenerateOffHeapTrampolineFor.
  friend class Isolate;
  friend class SetupIsolateDelegate;

  DISALLOW_COPY_AND_ASSIGN(Builtins);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_H_
